#pragma once

#include <stdint.h>

/* Flags de controle das entradas de tabelas de páginas x86 (PDE / PTE) */
#define VMM_PAGE_PRESENT 0x001U  // Bit 0: Página presente na memória RAM física
#define VMM_PAGE_RW      0x002U  // Bit 1: Permissão de Leitura/Escrita (1 = R/W, 0 = Read-Only)
#define VMM_PAGE_USER    0x004U  // Bit 2: Nível de privilégio (1 = User Mode / Ring 3, 0 = Supervisor / Ring 0)

/* Tamanho padrão de uma página virtual no x86 (4KB / 4096 bytes) */
#define VMM_PAGE_SIZE     0x1000U

/*
 * Limites da janela de endereçamento virtual para espaço de usuário em x86 (CZK_x86):
 * - 0x00000000 - 0x00FFFFFF: Reservado para bootstrap, mapeamentos do Kernel e Heap do Kernel.
 * - 0x01000000 - 0xFF800000: Espaço virtual disponível para carregamento de binários e pilhas de User Mode.
 * - PDEs 1022 e 1023: Reservados para a janela temporária de mapeamento físico e paginação recursiva.
 */
#define VMM_USER_MIN_ADDR 0x01000000U
#define VMM_USER_MAX_ADDR 0xFF800000U

/* Inicialização e verificação de estado do VMM */
void vmm_init(void);
uint8_t vmm_is_enabled(void);     // Verifica se o bit de paginação (PG - bit 31 do CR0) está ativo
uint8_t vmm_wp_is_enabled(void);  // Verifica se o bit de proteção contra escrita (WP - bit 16 do CR0) está ativo

/* Gerenciamento do registrador de controle de paginação (CR3) */
uint32_t vmm_kernel_cr3(void);                   // Retorna o CR3 do espaço de endereçamento do Kernel
uint32_t vmm_current_cr3(void);                  // Retorna o CR3 ativado no momento na CPU
void vmm_switch_address_space(uint32_t cr3);     // Carrega um novo CR3 na CPU (provoca flush do TLB)

/* Criação, destruição e telemetria de espaços de endereçamento virtuais */
uint32_t vmm_create_address_space(void);         // Clona o Page Directory básico do Kernel para um novo processo
void vmm_destroy_address_space(uint32_t cr3);    // Libera todas as tabelas de páginas e o Page Directory associado
uint32_t vmm_address_space_count(void);          // Retorna o número de espaços de endereçamento ativos no sistema

/* Operações na janela de mapeamento físico temporário */
uint8_t vmm_temp_window_is_ready(void);
int vmm_copy_to_phys(
    uintptr_t phys_addr,
    const void *src,
    uint32_t len
);
int vmm_copy_from_phys(
    void *dst,
    uintptr_t phys_addr,
    uint32_t len
);
int vmm_copy_phys(
    uintptr_t dst_phys,
    uintptr_t src_phys,
    uint32_t len
);
int vmm_zero_phys(uintptr_t phys_addr, uint32_t len);

/* Duplicação/Clonagem de faixas virtuais em espaços de endereçamento (ex: implementação de fork) */
int vmm_clone_user_range(
    uint32_t cr3,
    uintptr_t start,
    uintptr_t end,
    uint32_t flags
);

/* Mapeamento e consulta de páginas especificando um CR3 alvo */
int vmm_map_user_page(
    uint32_t cr3,
    uintptr_t virt_addr,
    uintptr_t phys_addr,
    uint32_t flags
);
int vmm_set_user_page_flags(
    uint32_t cr3,
    uintptr_t virt_addr,
    uint32_t flags
);
uintptr_t vmm_translate_address_space(
    uint32_t cr3,
    uintptr_t virt_addr
);
int vmm_get_page_flags_address_space(
    uint32_t cr3,
    uintptr_t virt_addr,
    uint32_t *flags_out
);

/* Mapeamento, desmapeamento e tradução direta no espaço de endereçamento atual */
int vmm_map_page(uintptr_t virt_addr, uintptr_t phys_addr, uint32_t flags);
int vmm_unmap_page(uintptr_t virt_addr);
uint8_t vmm_is_mapped(uintptr_t virt_addr);
uintptr_t vmm_translate(uintptr_t virt_addr);
int vmm_get_page_flags(uintptr_t virt_addr, uint32_t *flags_out);