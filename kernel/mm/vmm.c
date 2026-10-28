#include <kernel/vmm.h>
#include <kernel/pmm.h>

/* Tamanho de página padrão x86 (4KB) e total de entradas por Page Directory/Table */
#define PAGE_SIZE              0x1000U
#define PAGE_ENTRIES           1024U
#define PAGE_FRAME_MASK        0xFFFFF000U
#define PAGE_FLAGS_MASK        0x00000FFFU

/* Bits de controle do registrador CR0 (PG = Paging Enabled, WP = Write Protect) */
#define CR0_PG                 0x80000000U
#define CR0_WP                 0x00010000U

/* Tamanho da identidade mapeada na inicialização e máximo de espaços de usuário ativos */
#define VMM_BOOTSTRAP_MB       16U
#define VMM_MAX_ADDRESS_SPACES 32U

/*
 * Reserva a PDE 1022 (0xFF800000 - 0xFFBFFFFF) para mapeamentos físicos temporários.
 * A PDE 1023 permanece livre para esquemas futuros de paginação recursiva.
 */
#define VMM_TEMP_SRC_ADDR       0xFF800000U
#define VMM_TEMP_DST_ADDR       0xFF801000U
#define VMM_TEMP_DIR_INDEX      1022U

/* Page Directory estático do Kernel (alinhado em limite de página de 4KB) */
static uint32_t page_directory[PAGE_ENTRIES] __attribute__((aligned(4096)));
static uint32_t address_spaces[VMM_MAX_ADDRESS_SPACES];
static uint32_t address_space_count;
static uint8_t vmm_enabled;
static uint8_t temp_window_ready;

/* Zera um array de inteiros de 32 bits (4 bytes por palavra) */
static void mem_zero_u32(uint32_t *dst, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        dst[i] = 0U;
    }
}

/* Copia 'len' bytes entre dois endereços */
static void mem_copy_bytes(
    uint8_t *dst,
    const uint8_t *src,
    uint32_t len
) {
    for (uint32_t i = 0U; i < len; ++i) {
        dst[i] = src[i];
    }
}

/* Zera uma região de memória byte a byte */
static void mem_zero_bytes(uint8_t *dst, uint32_t len) {
    for (uint32_t i = 0U; i < len; ++i) {
        dst[i] = 0U;
    }
}

/* Retorna o menor valor entre dois inteiros sem sinal de 32 bits */
static uint32_t min_u32(uint32_t a, uint32_t b) {
    return (a < b) ? a : b;
}

/* Salva as EFLAGS, desativa interrupções de hardware (cli) e retorna o estado antigo */
static uint32_t vmm_irq_save_disable(void) {
    uint32_t flags;

    asm volatile (
        "pushf\n"
        "pop %0\n"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );

    return flags;
}

/* Restaura o estado anterior das interrupções checando o bit IF (Interrupt Flag) */
static void vmm_irq_restore(uint32_t flags) {
    if ((flags & (1U << 9)) != 0U) {
        asm volatile ("sti" : : : "memory");
    }
}

/* Invalida uma entrada na Translation Lookaside Buffer (TLB) da CPU para o endereço virtual dado */
static void vmm_invlpg(uintptr_t addr) {
    asm volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

/* Extrai o índice do Page Directory (os 10 bits mais significativos) de um endereço virtual */
static uint32_t vmm_dir_index(uintptr_t addr) {
    return (uint32_t)(addr >> 22);
}

/* Extrai o índice da Page Table (bits 12 a 21) de um endereço virtual */
static uint32_t vmm_table_index(uintptr_t addr) {
    return (uint32_t)((addr >> 12) & 0x3FFU);
}

/* Obtém o ponteiro para o Page Directory removendo as flags do endereço de CR3 */
static uint32_t *vmm_directory_from_cr3(uint32_t cr3) {
    return (uint32_t *)(uintptr_t)(cr3 & PAGE_FRAME_MASK);
}

/* Lê o valor atual do registrador de controle CR3 (ponteiro para o Page Directory atual) */
static uint32_t vmm_read_cr3(void) {
    uint32_t cr3;

    asm volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3 & PAGE_FRAME_MASK;
}

/* Retorna o ponteiro para a Page Table se a entrada no Page Directory (PDE) estiver presente */
static uint32_t *vmm_get_table_from_directory(
    uint32_t *directory,
    uint32_t dir_idx
) {
    uint32_t pde;

    if (directory == 0 || dir_idx >= PAGE_ENTRIES) {
        return 0;
    }

    pde = directory[dir_idx];
    if ((pde & VMM_PAGE_PRESENT) == 0U) {
        return 0;
    }

    return (uint32_t *)(uintptr_t)(pde & PAGE_FRAME_MASK);
}

/* Garante a existência de uma Page Table para o índice de diretório fornecido, alocando se necessário */
static uint32_t *vmm_ensure_table_for_directory(
    uint32_t *directory,
    uint32_t dir_idx
) {
    uint32_t frame;
    uint32_t *table;

    if (directory == 0 || dir_idx >= PAGE_ENTRIES) {
        return 0;
    }

    table = vmm_get_table_from_directory(directory, dir_idx);
    if (table != 0) {
        return table;
    }

    /* Aloca um frame de memória física para a nova Page Table */
    frame = pmm_alloc_table_frame();
    if (frame == 0U) {
        return 0;
    }

    table = (uint32_t *)(uintptr_t)frame;
    mem_zero_u32(table, PAGE_ENTRIES);

    directory[dir_idx] =
        (frame & PAGE_FRAME_MASK) |
        VMM_PAGE_PRESENT |
        VMM_PAGE_RW;

    return table;
}

/* Prepara a janela virtual reservada para cópias e operações temporárias na memória física */
static int vmm_prepare_temp_window(void) {
    uint32_t *table;

    table = vmm_ensure_table_for_directory(
        page_directory,
        VMM_TEMP_DIR_INDEX
    );
    if (table == 0) {
        return -1;
    }

    table[vmm_table_index(VMM_TEMP_SRC_ADDR)] = 0U;
    table[vmm_table_index(VMM_TEMP_DST_ADDR)] = 0U;
    temp_window_ready = 1U;
    return 0;
}

/* Mapeia temporariamente um frame físico em um dos slots virtuais de trabalho */
static int vmm_temp_map_slot(
    uintptr_t virt_addr,
    uintptr_t phys_addr,
    uint32_t flags
) {
    uint32_t *directory;
    uint32_t *table;
    uint32_t table_idx;

    if (!vmm_enabled ||
        !temp_window_ready ||
        vmm_dir_index(virt_addr) != VMM_TEMP_DIR_INDEX) {
        return -1;
    }

    directory =
        vmm_directory_from_cr3(vmm_read_cr3());
    table =
        vmm_get_table_from_directory(
            directory,
            VMM_TEMP_DIR_INDEX
        );
    if (table == 0) {
        return -1;
    }

    table_idx = vmm_table_index(virt_addr);
    table[table_idx] =
        ((uint32_t)phys_addr & PAGE_FRAME_MASK) |
        VMM_PAGE_PRESENT |
        (flags & VMM_PAGE_RW);

    vmm_invlpg(virt_addr);
    return 0;
}

/* Desfaz o mapeamento temporário e invalida a página na TLB */
static void vmm_temp_unmap_slot(uintptr_t virt_addr) {
    uint32_t *directory;
    uint32_t *table;

    if (!vmm_enabled ||
        !temp_window_ready ||
        vmm_dir_index(virt_addr) != VMM_TEMP_DIR_INDEX) {
        return;
    }

    directory =
        vmm_directory_from_cr3(vmm_read_cr3());
    table =
        vmm_get_table_from_directory(
            directory,
            VMM_TEMP_DIR_INDEX
        );
    if (table == 0) {
        return;
    }

    table[vmm_table_index(virt_addr)] = 0U;
    vmm_invlpg(virt_addr);
}

/* Mapeia uma página física em um endereço virtual dentro de um diretório de paginação específico */
static int vmm_map_page_in_directory(
    uint32_t *directory,
    uintptr_t virt_addr,
    uintptr_t phys_addr,
    uint32_t flags
) {
    uint32_t dir_idx;
    uint32_t table_idx;
    uint32_t page_flags;
    uint32_t *table;

    if (directory == 0 ||
        (virt_addr & (PAGE_SIZE - 1U)) != 0U ||
        (phys_addr & (PAGE_SIZE - 1U)) != 0U) {
        return -2;
    }

    dir_idx = vmm_dir_index(virt_addr);
    table_idx = vmm_table_index(virt_addr);
    page_flags =
        VMM_PAGE_PRESENT |
        (flags & (VMM_PAGE_RW | VMM_PAGE_USER));

    table = vmm_ensure_table_for_directory(directory, dir_idx);
    if (table == 0) {
        return -1;
    }

    /* Propaga a permissão de Usuário (Ring 3) para o diretório se requisitada */
    if ((flags & VMM_PAGE_USER) != 0U) {
        directory[dir_idx] |= VMM_PAGE_USER;
    }

    table[table_idx] =
        ((uint32_t)phys_addr & PAGE_FRAME_MASK) |
        page_flags;

    if (vmm_enabled &&
        directory == vmm_directory_from_cr3(vmm_read_cr3())) {
        vmm_invlpg(virt_addr);
    }

    return 0;
}

/* Desfaz o mapeamento de uma página virtual e libera a Page Table caso ela fique vazia */
static int vmm_unmap_page_in_directory(
    uint32_t *directory,
    uintptr_t virt_addr
) {
    uint32_t dir_idx;
    uint32_t table_idx;
    uint32_t *table;
    uint8_t empty = 1U;

    if (directory == 0 ||
        (virt_addr & (PAGE_SIZE - 1U)) != 0U) {
        return -2;
    }

    dir_idx = vmm_dir_index(virt_addr);
    table_idx = vmm_table_index(virt_addr);

    table = vmm_get_table_from_directory(directory, dir_idx);
    if (table == 0) {
        return -1;
    }

    table[table_idx] = 0U;

    if (vmm_enabled &&
        directory == vmm_directory_from_cr3(vmm_read_cr3())) {
        vmm_invlpg(virt_addr);
    }

    /* Verifica se ainda há alguma página mapeada nessa Page Table */
    for (uint32_t i = 0U; i < PAGE_ENTRIES; ++i) {
        if ((table[i] & VMM_PAGE_PRESENT) != 0U) {
            empty = 0U;
            break;
        }
    }

    /* Libera o frame da Page Table no PMM se não houver mais páginas ativas nela */
    if (empty) {
        uint32_t table_phys =
            directory[dir_idx] & PAGE_FRAME_MASK;

        directory[dir_idx] = 0U;
        pmm_free_table_frame(table_phys);
    }

    return 0;
}

/* Traduz um endereço virtual para endereço físico baseado nas tabelas de um diretório específico */
static uintptr_t vmm_translate_in_directory(
    uint32_t *directory,
    uintptr_t virt_addr
) {
    uint32_t dir_idx;
    uint32_t table_idx;
    uint32_t *table;
    uint32_t pte;

    if (directory == 0) {
        return 0U;
    }

    dir_idx = vmm_dir_index(virt_addr);
    table_idx = vmm_table_index(virt_addr);

    table = vmm_get_table_from_directory(directory, dir_idx);
    if (table == 0) {
        return 0U;
    }

    pte = table[table_idx];
    if ((pte & VMM_PAGE_PRESENT) == 0U) {
        return 0U;
    }

    return
        (uintptr_t)(pte & PAGE_FRAME_MASK) |
        (virt_addr & (PAGE_SIZE - 1U));
}

/* Obtém as flags efetivas de proteção (PRESENT, RW, USER) de um endereço virtual */
static int vmm_get_page_flags_in_directory(
    uint32_t *directory,
    uintptr_t virt_addr,
    uint32_t *flags_out
) {
    uint32_t dir_idx;
    uint32_t table_idx;
    uint32_t *table;
    uint32_t pde;
    uint32_t pte;
    uint32_t flags;

    if (directory == 0 || flags_out == 0) {
        return -1;
    }

    dir_idx = vmm_dir_index(virt_addr);
    table_idx = vmm_table_index(virt_addr);

    if (dir_idx >= PAGE_ENTRIES) {
        return -1;
    }

    pde = directory[dir_idx];
    if ((pde & VMM_PAGE_PRESENT) == 0U) {
        return -1;
    }

    table = vmm_get_table_from_directory(directory, dir_idx);
    if (table == 0) {
        return -1;
    }

    pte = table[table_idx];
    if ((pte & VMM_PAGE_PRESENT) == 0U) {
        return -1;
    }

    flags = VMM_PAGE_PRESENT;

    /* A permissão final é a interseção entre o Page Directory Entry e a Page Table Entry */
    if ((pde & VMM_PAGE_USER) != 0U &&
        (pte & VMM_PAGE_USER) != 0U) {
        flags |= VMM_PAGE_USER;
    }

    if ((pde & VMM_PAGE_RW) != 0U &&
        (pte & VMM_PAGE_RW) != 0U) {
        flags |= VMM_PAGE_RW;
    }

    *flags_out = flags;
    return 0;
}

/* Altera as flags de controle de uma página mantendo a base física */
static int vmm_set_page_flags_in_directory(
    uint32_t *directory,
    uintptr_t virt_addr,
    uint32_t flags
) {
    uint32_t dir_idx;
    uint32_t table_idx;
    uint32_t *table;
    uint32_t pte;

    if (directory == 0 ||
        (virt_addr & (PAGE_SIZE - 1U)) != 0U) {
        return -1;
    }

    dir_idx = vmm_dir_index(virt_addr);
    table_idx = vmm_table_index(virt_addr);
    table = vmm_get_table_from_directory(directory, dir_idx);

    if (table == 0) {
        return -1;
    }

    pte = table[table_idx];
    if ((pte & VMM_PAGE_PRESENT) == 0U) {
        return -1;
    }

    table[table_idx] =
        (pte & PAGE_FRAME_MASK) |
        VMM_PAGE_PRESENT |
        (flags & (VMM_PAGE_RW | VMM_PAGE_USER));

    if ((flags & VMM_PAGE_USER) != 0U) {
        directory[dir_idx] |= VMM_PAGE_USER;
    }

    if (vmm_enabled &&
        directory == vmm_directory_from_cr3(vmm_read_cr3())) {
        vmm_invlpg(virt_addr);
    }

    return 0;
}

/* Registra um novo espaço de endereçamento (CR3 de processo de usuário) na tabela do VMM */
static int vmm_register_address_space(uint32_t cr3) {
    if (address_space_count >= VMM_MAX_ADDRESS_SPACES) {
        return -1;
    }

    address_spaces[address_space_count++] =
        cr3 & PAGE_FRAME_MASK;

    return 0;
}

/* Remove um espaço de endereçamento registrado da tabela do VMM */
static void vmm_unregister_address_space(uint32_t cr3) {
    uint32_t target = cr3 & PAGE_FRAME_MASK;

    for (uint32_t i = 0U; i < address_space_count; ++i) {
        if (address_spaces[i] == target) {
            for (uint32_t j = i + 1U;
                 j < address_space_count;
                 ++j) {
                address_spaces[j - 1U] =
                    address_spaces[j];
            }

            address_space_count--;
            address_spaces[address_space_count] = 0U;
            return;
        }
    }
}

/* Verifica se determinado CR3 está cadastrado como espaço de endereçamento ativo */
static uint8_t vmm_address_space_registered(uint32_t cr3) {
    uint32_t target = cr3 & PAGE_FRAME_MASK;

    for (uint32_t i = 0U; i < address_space_count; ++i) {
        if (address_spaces[i] == target) {
            return 1U;
        }
    }

    return 0U;
}

/* Mapeia diretamente (Identity Map) os primeiros 'megabytes' para inicialização do Kernel */
static void vmm_map_identity(uint32_t megabytes) {
    uintptr_t limit =
        (uintptr_t)megabytes * 1024U * 1024U;

    if (megabytes > VMM_BOOTSTRAP_MB) {
        limit =
            (uintptr_t)VMM_BOOTSTRAP_MB *
            1024U *
            1024U;
    }

    mem_zero_u32(page_directory, PAGE_ENTRIES);

    for (uintptr_t addr = PAGE_SIZE;
         addr < limit;
         addr += PAGE_SIZE) {
        (void)vmm_map_page_in_directory(
            page_directory,
            addr,
            addr,
            VMM_PAGE_RW
        );
    }
}

/**
 * Inicializa o subsistema de memória virtual (VMM).
 * Mapeia os primeiros 16MB em identidade, ativa a paginação de 32 bits configurando
 * os bits PG e WP no registrador CR0 e carrega o Page Directory padrão no CR3.
 */
void vmm_init(void) {
    uint32_t cr0;

    address_space_count = 0U;
    temp_window_ready = 0U;
    for (uint32_t i = 0U;
         i < VMM_MAX_ADDRESS_SPACES;
         ++i) {
        address_spaces[i] = 0U;
    }

    vmm_map_identity(VMM_BOOTSTRAP_MB);
    (void)vmm_prepare_temp_window();

    /* Carrega o endereço do Page Directory do Kernel no registrador CR3 */
    asm volatile (
        "mov %0, %%cr3"
        :
        : "r"((uint32_t)(uintptr_t)page_directory)
        : "memory"
    );

    /* Habilita Paginação (PG) e Proteção de Escrita no Ring 0 (WP) no CR0 */
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (CR0_PG | CR0_WP);
    asm volatile (
        "mov %0, %%cr0"
        :
        : "r"(cr0)
        : "memory"
    );

    vmm_enabled = 1U;
}

/* Informa se o subsistema de paginação virtual foi ativado */
uint8_t vmm_is_enabled(void) {
    return vmm_enabled;
}

/* Informa se o bit de Write Protect (CR0.WP) está ativado */
uint8_t vmm_wp_is_enabled(void) {
    uint32_t cr0;

    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    return (uint8_t)((cr0 & CR0_WP) != 0U);
}

/* Retorna o valor de CR3 pertencente ao espaço de endereçamento do Kernel */
uint32_t vmm_kernel_cr3(void) {
    return
        (uint32_t)(uintptr_t)page_directory &
        PAGE_FRAME_MASK;
}

/* Retorna o CR3 atualmente carregado na CPU */
uint32_t vmm_current_cr3(void) {
    if (!vmm_enabled) {
        return 0U;
    }

    return vmm_read_cr3();
}

/* Alterna o espaço de endereçamento ativo recarregando o registrador CR3 */
void vmm_switch_address_space(uint32_t cr3) {
    uint32_t target;

    if (!vmm_enabled) {
        return;
    }

    target =
        (cr3 != 0U)
            ? (cr3 & PAGE_FRAME_MASK)
            : vmm_kernel_cr3();

    if (target == vmm_read_cr3()) {
        return;
    }

    asm volatile (
        "mov %0, %%cr3"
        :
        : "r"(target)
        : "memory"
    );
}

/**
 * Cria um novo espaço de endereçamento virtual para um processo de usuário.
 * Duplica as tabelas do espaço de endereçamento do Kernel garantindo que o Ring 0
 * seja compartilhado e acessível por todas as tarefas.
 */
uint32_t vmm_create_address_space(void) {
    uint32_t directory_frame;
    uint32_t *directory;

    if (!vmm_enabled) {
        return 0U;
    }

    directory_frame = pmm_alloc_table_frame();
    if (directory_frame == 0U) {
        return 0U;
    }

    directory =
        (uint32_t *)(uintptr_t)directory_frame;
    mem_zero_u32(directory, PAGE_ENTRIES);

    for (uint32_t dir_idx = 0U;
         dir_idx < PAGE_ENTRIES;
         ++dir_idx) {
        uint32_t src_pde = page_directory[dir_idx];
        uint32_t table_frame;
        uint32_t *src_table;
        uint32_t *dst_table;

        if ((src_pde & VMM_PAGE_PRESENT) == 0U) {
            continue;
        }

        table_frame = pmm_alloc_table_frame();
        if (table_frame == 0U) {
            vmm_destroy_address_space(directory_frame);
            return 0U;
        }

        src_table =
            (uint32_t *)(uintptr_t)
            (src_pde & PAGE_FRAME_MASK);
        dst_table =
            (uint32_t *)(uintptr_t)table_frame;

        /* Copia mapeamentos garantindo a remoção do bit de usuário em áreas restritas */
        for (uint32_t i = 0U; i < PAGE_ENTRIES; ++i) {
            dst_table[i] =
                src_table[i] & ~VMM_PAGE_USER;
        }

        directory[dir_idx] =
            (table_frame & PAGE_FRAME_MASK) |
            ((src_pde & PAGE_FLAGS_MASK) &
             ~VMM_PAGE_USER);
    }

    if (vmm_register_address_space(directory_frame) != 0) {
        vmm_destroy_address_space(directory_frame);
        return 0U;
    }

    return directory_frame;
}

/* Destrói um espaço de endereçamento, liberando as páginas exclusivas de usuário e suas Page Tables */
void vmm_destroy_address_space(uint32_t cr3) {
    uint32_t target = cr3 & PAGE_FRAME_MASK;
    uint32_t *directory;

    if (target == 0U || target == vmm_kernel_cr3()) {
        return;
    }

    if (vmm_enabled && target == vmm_read_cr3()) {
        vmm_switch_address_space(vmm_kernel_cr3());
    }

    directory = vmm_directory_from_cr3(target);
    if (directory == 0) {
        return;
    }

    vmm_unregister_address_space(target);

    for (uint32_t dir_idx = 0U;
         dir_idx < PAGE_ENTRIES;
         ++dir_idx) {
        uint32_t pde = directory[dir_idx];
        uint32_t *table;

        if ((pde & VMM_PAGE_PRESENT) == 0U) {
            continue;
        }

        table =
            (uint32_t *)(uintptr_t)
            (pde & PAGE_FRAME_MASK);

        /* Devolve os frames exclusivos de espaço de usuário ao PMM */
        for (uint32_t i = 0U; i < PAGE_ENTRIES; ++i) {
            uint32_t pte = table[i];

            if ((pte & (VMM_PAGE_PRESENT | VMM_PAGE_USER)) ==
                (VMM_PAGE_PRESENT | VMM_PAGE_USER)) {
                pmm_free_frame(pte & PAGE_FRAME_MASK);
                table[i] = 0U;
            }
        }

        pmm_free_table_frame(pde & PAGE_FRAME_MASK);
        directory[dir_idx] = 0U;
    }

    pmm_free_table_frame(target);
}

/* Retorna o número atual de espaços de endereçamento registrados */
uint32_t vmm_address_space_count(void) {
    return address_space_count;
}

/* Retorna 1 se a janela de mapeamento temporário estiver pronta para ser utilizada */
uint8_t vmm_temp_window_is_ready(void) {
    return temp_window_ready;
}

/* Copia dados de um buffer virtual no Kernel diretamente para um endereço físico */
int vmm_copy_to_phys(
    uintptr_t phys_addr,
    const void *src,
    uint32_t len
) {
    const uint8_t *source = (const uint8_t *)src;
    uint32_t flags;

    if (len == 0U) {
        return 0;
    }

    if (source == 0 || !temp_window_ready) {
        return -1;
    }

    flags = vmm_irq_save_disable();

    while (len != 0U) {
        uintptr_t frame =
            phys_addr & (uintptr_t)PAGE_FRAME_MASK;
        uint32_t offset =
            (uint32_t)(phys_addr & (PAGE_SIZE - 1U));
        uint32_t chunk =
            min_u32(len, PAGE_SIZE - offset);

        if (vmm_temp_map_slot(
                VMM_TEMP_DST_ADDR,
                frame,
                VMM_PAGE_RW) != 0) {
            vmm_irq_restore(flags);
            return -2;
        }

        mem_copy_bytes(
            (uint8_t *)(uintptr_t)
                (VMM_TEMP_DST_ADDR + offset),
            source,
            chunk
        );

        vmm_temp_unmap_slot(VMM_TEMP_DST_ADDR);

        source += chunk;
        phys_addr += chunk;
        len -= chunk;
    }

    vmm_irq_restore(flags);
    return 0;
}

/* Copia dados de um endereço físico de memória para um buffer virtual no Kernel */
int vmm_copy_from_phys(
    void *dst,
    uintptr_t phys_addr,
    uint32_t len
) {
    uint8_t *dest = (uint8_t *)dst;
    uint32_t flags;

    if (len == 0U) {
        return 0;
    }

    if (dest == 0 || !temp_window_ready) {
        return -1;
    }

    flags = vmm_irq_save_disable();

    while (len != 0U) {
        uintptr_t frame =
            phys_addr & (uintptr_t)PAGE_FRAME_MASK;
        uint32_t offset =
            (uint32_t)(phys_addr & (PAGE_SIZE - 1U));
        uint32_t chunk =
            min_u32(len, PAGE_SIZE - offset);

        if (vmm_temp_map_slot(
                VMM_TEMP_SRC_ADDR,
                frame,
                0U) != 0) {
            vmm_irq_restore(flags);
            return -2;
        }

        mem_copy_bytes(
            dest,
            (const uint8_t *)(uintptr_t)
                (VMM_TEMP_SRC_ADDR + offset),
            chunk
        );

        vmm_temp_unmap_slot(VMM_TEMP_SRC_ADDR);

        dest += chunk;
        phys_addr += chunk;
        len -= chunk;
    }

    vmm_irq_restore(flags);
    return 0;
}

/* Copia dados diretamente entre dois endereços físicos de memória utilizando as duas janelas temporárias */
int vmm_copy_phys(
    uintptr_t dst_phys,
    uintptr_t src_phys,
    uint32_t len
) {
    uint32_t flags;

    if (len == 0U) {
        return 0;
    }

    if (!temp_window_ready) {
        return -1;
    }

    flags = vmm_irq_save_disable();

    while (len != 0U) {
        uintptr_t src_frame =
            src_phys & (uintptr_t)PAGE_FRAME_MASK;
        uintptr_t dst_frame =
            dst_phys & (uintptr_t)PAGE_FRAME_MASK;
        uint32_t src_offset =
            (uint32_t)(src_phys & (PAGE_SIZE - 1U));
        uint32_t dst_offset =
            (uint32_t)(dst_phys & (PAGE_SIZE - 1U));
        uint32_t chunk =
            min_u32(
                len,
                min_u32(
                    PAGE_SIZE - src_offset,
                    PAGE_SIZE - dst_offset
                )
            );

        if (vmm_temp_map_slot(
                VMM_TEMP_SRC_ADDR,
                src_frame,
                0U) != 0) {
            vmm_irq_restore(flags);
            return -2;
        }

        if (vmm_temp_map_slot(
                VMM_TEMP_DST_ADDR,
                dst_frame,
                VMM_PAGE_RW) != 0) {
            vmm_temp_unmap_slot(VMM_TEMP_SRC_ADDR);
            vmm_irq_restore(flags);
            return -3;
        }

        mem_copy_bytes(
            (uint8_t *)(uintptr_t)
                (VMM_TEMP_DST_ADDR + dst_offset),
            (const uint8_t *)(uintptr_t)
                (VMM_TEMP_SRC_ADDR + src_offset),
            chunk
        );

        vmm_temp_unmap_slot(VMM_TEMP_DST_ADDR);
        vmm_temp_unmap_slot(VMM_TEMP_SRC_ADDR);

        dst_phys += chunk;
        src_phys += chunk;
        len -= chunk;
    }

    vmm_irq_restore(flags);
    return 0;
}

/* Zera uma área de memória física preenchendo os frames correspondentes com zero */
int vmm_zero_phys(uintptr_t phys_addr, uint32_t len) {
    uint32_t flags;

    if (len == 0U) {
        return 0;
    }

    if (!temp_window_ready) {
        return -1;
    }

    flags = vmm_irq_save_disable();

    while (len != 0U) {
        uintptr_t frame =
            phys_addr & (uintptr_t)PAGE_FRAME_MASK;
        uint32_t offset =
            (uint32_t)(phys_addr & (PAGE_SIZE - 1U));
        uint32_t chunk =
            min_u32(len, PAGE_SIZE - offset);

        if (vmm_temp_map_slot(
                VMM_TEMP_DST_ADDR,
                frame,
                VMM_PAGE_RW) != 0) {
            vmm_irq_restore(flags);
            return -2;
        }

        mem_zero_bytes(
            (uint8_t *)(uintptr_t)
                (VMM_TEMP_DST_ADDR + offset),
            chunk
        );

        vmm_temp_unmap_slot(VMM_TEMP_DST_ADDR);

        phys_addr += chunk;
        len -= chunk;
    }

    vmm_irq_restore(flags);
    return 0;
}

/* Clona uma faixa de memória de espaço de usuário de um processo pai para um novo espaço (fork) */
int vmm_clone_user_range(
    uint32_t cr3,
    uintptr_t start,
    uintptr_t end,
    uint32_t flags
) {
    uintptr_t page_start;
    uintptr_t page_end;
    uint32_t *directory;

    if (cr3 == 0U ||
        (cr3 & PAGE_FRAME_MASK) == vmm_kernel_cr3() ||
        start >= end) {
        return -1;
    }

    directory =
        vmm_directory_from_cr3(cr3);
    page_start =
        start & ~(uintptr_t)(PAGE_SIZE - 1U);
    page_end =
        (end + PAGE_SIZE - 1U) &
        ~(uintptr_t)(PAGE_SIZE - 1U);

    for (uintptr_t addr = page_start;
         addr < page_end;
         addr += PAGE_SIZE) {
        uintptr_t source_phys =
            vmm_translate_in_directory(
                page_directory,
                addr
            );
        uint32_t frame;

        if (source_phys == 0U) {
            return -3;
        }

        frame = pmm_alloc_frame();
        if (frame == 0U) {
            return -4;
        }

        if (vmm_copy_phys(
                frame,
                source_phys & PAGE_FRAME_MASK,
                PAGE_SIZE) != 0) {
            pmm_free_frame(frame);
            return -6;
        }

        if (vmm_map_page_in_directory(
                directory,
                addr,
                frame,
                flags | VMM_PAGE_USER) != 0) {
            pmm_free_frame(frame);
            return -5;
        }
    }

    return 0;
}

/* Mapeia uma página física explicitamente no espaço de memória de usuário (Ring 3) do CR3 informado */
int vmm_map_user_page(
    uint32_t cr3,
    uintptr_t virt_addr,
    uintptr_t phys_addr,
    uint32_t flags
) {
    uint32_t target = cr3 & PAGE_FRAME_MASK;
    uint32_t *directory;

    if (target == 0U ||
        target == vmm_kernel_cr3() ||
        !vmm_address_space_registered(target)) {
        return -1;
    }

    if ((virt_addr & (PAGE_SIZE - 1U)) != 0U ||
        (phys_addr & (PAGE_SIZE - 1U)) != 0U ||
        virt_addr < VMM_USER_MIN_ADDR ||
        virt_addr >= VMM_USER_MAX_ADDR) {
        return -2;
    }

    directory = vmm_directory_from_cr3(target);

    if (vmm_translate_in_directory(directory, virt_addr) != 0U) {
        return -3;
    }

    return vmm_map_page_in_directory(
        directory,
        virt_addr,
        phys_addr,
        VMM_PAGE_USER | (flags & VMM_PAGE_RW)
    );
}

/* Atualiza as permissões de acesso de uma página no espaço de usuário do CR3 fornecido */
int vmm_set_user_page_flags(
    uint32_t cr3,
    uintptr_t virt_addr,
    uint32_t flags
) {
    uint32_t target = cr3 & PAGE_FRAME_MASK;
    uint32_t *directory;
    uint32_t current_flags;

    if (target == 0U ||
        target == vmm_kernel_cr3() ||
        !vmm_address_space_registered(target)) {
        return -1;
    }

    if ((virt_addr & (PAGE_SIZE - 1U)) != 0U ||
        virt_addr < VMM_USER_MIN_ADDR ||
        virt_addr >= VMM_USER_MAX_ADDR) {
        return -2;
    }

    directory = vmm_directory_from_cr3(target);

    if (vmm_get_page_flags_in_directory(
            directory,
            virt_addr,
            &current_flags) != 0 ||
        (current_flags & VMM_PAGE_USER) == 0U) {
        return -3;
    }

    return vmm_set_page_flags_in_directory(
        directory,
        virt_addr,
        VMM_PAGE_USER | (flags & VMM_PAGE_RW)
    );
}

/* Traduz um endereço virtual para endereço físico em um espaço de endereçamento (CR3) específico */
uintptr_t vmm_translate_address_space(
    uint32_t cr3,
    uintptr_t virt_addr
) {
    uint32_t target = cr3 & PAGE_FRAME_MASK;

    if (target == vmm_kernel_cr3()) {
        return vmm_translate_in_directory(
            page_directory,
            virt_addr
        );
    }

    if (target == 0U ||
        !vmm_address_space_registered(target)) {
        return 0U;
    }

    return vmm_translate_in_directory(
        vmm_directory_from_cr3(target),
        virt_addr
    );
}

/* Consulta as flags de uma página dentro de determinado espaço de endereçamento (CR3) */
int vmm_get_page_flags_address_space(
    uint32_t cr3,
    uintptr_t virt_addr,
    uint32_t *flags_out
) {
    uint32_t target = cr3 & PAGE_FRAME_MASK;

    if (flags_out == 0) {
        return -1;
    }

    if (target == vmm_kernel_cr3()) {
        return vmm_get_page_flags_in_directory(
            page_directory,
            virt_addr,
            flags_out
        );
    }

    if (target == 0U ||
        !vmm_address_space_registered(target)) {
        return -1;
    }

    return vmm_get_page_flags_in_directory(
        vmm_directory_from_cr3(target),
        virt_addr,
        flags_out
    );
}

/* Mapeia uma página física no espaço de endereçamento do Kernel e sincroniza com os processos */
int vmm_map_page(
    uintptr_t virt_addr,
    uintptr_t phys_addr,
    uint32_t flags
) {
    int result;

    if (vmm_dir_index(virt_addr) == VMM_TEMP_DIR_INDEX) {
        return -3;
    }

    result = vmm_map_page_in_directory(
        page_directory,
        virt_addr,
        phys_addr,
        flags
    );

    if (result != 0) {
        return result;
    }

    if ((flags & VMM_PAGE_USER) != 0U) {
        return 0;
    }

    /* Propaga o novo mapeamento global do Kernel para todos os espaços de endereçamento ativos */
    for (uint32_t i = 0U;
         i < address_space_count;
         ++i) {
        uint32_t *directory =
            vmm_directory_from_cr3(address_spaces[i]);

        if (vmm_map_page_in_directory(
                directory,
                virt_addr,
                phys_addr,
                flags) != 0) {
            return -1;
        }
    }

    return 0;
}

/* Desfaz o mapeamento de uma página virtual no Kernel e propaga a remoção para os espaços ativos */
int vmm_unmap_page(uintptr_t virt_addr) {
    uint32_t flags = 0U;
    int kernel_result;

    if (vmm_dir_index(virt_addr) == VMM_TEMP_DIR_INDEX) {
        return -3;
    }

    if ((virt_addr & (PAGE_SIZE - 1U)) != 0U) {
        return -2;
    }

    (void)vmm_get_page_flags_in_directory(
        page_directory,
        virt_addr,
        &flags
    );

    kernel_result =
        vmm_unmap_page_in_directory(
            page_directory,
            virt_addr
        );

    if (kernel_result != 0) {
        return kernel_result;
    }

    if ((flags & VMM_PAGE_USER) != 0U) {
        return 0;
    }

    for (uint32_t i = 0U;
         i < address_space_count;
         ++i) {
        uint32_t *directory =
            vmm_directory_from_cr3(address_spaces[i]);

        (void)vmm_unmap_page_in_directory(
            directory,
            virt_addr
        );
    }

    return 0;
}

/* Retorna 1 se o endereço virtual especificado estiver mapeado no CR3 atual */
uint8_t vmm_is_mapped(uintptr_t virt_addr) {
    return (uint8_t)(vmm_translate(virt_addr) != 0U);
}

/* Traduz o endereço virtual para físico no espaço de endereçamento atualmente carregado na CPU */
uintptr_t vmm_translate(uintptr_t virt_addr) {
    uint32_t *directory;

    if (!vmm_enabled) {
        directory = page_directory;
    } else {
        directory =
            vmm_directory_from_cr3(vmm_read_cr3());
    }

    return vmm_translate_in_directory(
        directory,
        virt_addr
    );
}

/* Obtém as flags da página no espaço de endereçamento atualmente ativo na CPU */
int vmm_get_page_flags(
    uintptr_t virt_addr,
    uint32_t *flags_out
) {
    uint32_t *directory;

    if (!vmm_enabled) {
        directory = page_directory;
    } else {
        directory =
            vmm_directory_from_cr3(vmm_read_cr3());
    }

    return vmm_get_page_flags_in_directory(
        directory,
        virt_addr,
        flags_out
    );
}