#include <kernel/pmm.h>
#include <kernel/multiboot2.h>

/* Limites de memória gerenciada (512MB) e quantidade máxima de frames de 4KB */
#define PMM_MAX_MEMORY (512U * 1024U * 1024U)
#define PMM_MAX_FRAMES (PMM_MAX_MEMORY / PMM_FRAME_SIZE)

/* Limite físico máximo para alocação de tabelas de paginação (12MB) */
#define PMM_TABLE_FRAME_LIMIT 0x00C00000U

/* Bitmaps de estado: uso geral de frames e marcação específica de tabelas de páginas */
static uint8_t frame_bitmap[PMM_MAX_FRAMES / 8];
static uint8_t table_frame_bitmap[PMM_MAX_FRAMES / 8];
static uint32_t total_frames;
static uint32_t free_frames;

/* Define o bit correspondente ao frame como ocupado (1) */
static void set_bit(uint8_t *bitmap, uint32_t frame) {
    bitmap[frame / 8] |= (uint8_t)(1U << (frame % 8));
}

/* Limpa o bit correspondente ao frame deixando-o livre (0) */
static void clear_bit(uint8_t *bitmap, uint32_t frame) {
    bitmap[frame / 8] &= (uint8_t)~(1U << (frame % 8));
}

/* Retorna se o bit de um frame está definido */
static uint8_t test_bit(const uint8_t *bitmap, uint32_t frame) {
    return (uint8_t)(bitmap[frame / 8] & (1U << (frame % 8)));
}

/**
 * Marca uma faixa de endereços físicos como livre (used = 0) ou ocupada (used = 1),
 * ajustando os limites para o alinhamento de página de 4KB (PMM_FRAME_SIZE).
 */
static void pmm_mark_range(uintptr_t start, uintptr_t end, uint8_t used) {
    uintptr_t aligned_start = start & ~(uintptr_t)(PMM_FRAME_SIZE - 1U);
    uintptr_t aligned_end = (end + PMM_FRAME_SIZE - 1U) & ~(uintptr_t)(PMM_FRAME_SIZE - 1U);

    if (aligned_end > (uintptr_t)(total_frames * PMM_FRAME_SIZE)) {
        aligned_end = (uintptr_t)(total_frames * PMM_FRAME_SIZE);
    }

    for (uintptr_t addr = aligned_start; addr < aligned_end; addr += PMM_FRAME_SIZE) {
        uint32_t frame = (uint32_t)(addr / PMM_FRAME_SIZE);
        if (frame >= total_frames) {
            break;
        }

        if (used) {
            if (!test_bit(frame_bitmap, frame)) {
                set_bit(frame_bitmap, frame);
                free_frames--;
            }
        } else {
            if (test_bit(frame_bitmap, frame)) {
                clear_bit(frame_bitmap, frame);
                free_frames++;
            }
        }

        clear_bit(table_frame_bitmap, frame);
    }
}

/* Inicialização genérica do PMM assumindo memória contígua a partir de 1MB */
void pmm_init(uint32_t memory_top_bytes) {
    if (memory_top_bytes > PMM_MAX_MEMORY) {
        memory_top_bytes = PMM_MAX_MEMORY;
    }

    total_frames = memory_top_bytes / PMM_FRAME_SIZE;
    free_frames = 0;

    /* Marca inicialmente toda a memória como reservada/ocupada (0xFF) */
    for (uint32_t i = 0; i < sizeof(frame_bitmap); ++i) {
        frame_bitmap[i] = 0xFF;
        table_frame_bitmap[i] = 0x00;
    }

    if (total_frames == 0) {
        return;
    }

    /* Libera a faixa de memória acima de 1MB */
    pmm_mark_range(0x100000U, (uintptr_t)memory_top_bytes, 0);
}

/**
 * Inicializa o PMM analisando o mapa de memória física do Multiboot2 enviado pelo GRUB.
 * Reserva a área do Kernel, as estruturas do Multiboot2, o primeiro 1MB e os módulos (initrd).
 */
void pmm_init_from_multiboot(uint32_t mb_info_addr, uintptr_t kernel_start, uintptr_t kernel_end) {
    uint32_t total_size = *(uint32_t *)(uintptr_t)mb_info_addr;
    uintptr_t info_end = (uintptr_t)mb_info_addr + (uintptr_t)total_size;
    struct multiboot2_tag *tag = (struct multiboot2_tag *)((uintptr_t)mb_info_addr + 8U);
    uint32_t max_addr = 0;

    pmm_init(PMM_MAX_MEMORY);

    /* Passagem 1: Mapeia as regiões de memória RAM disponíveis informadas pela BIOS/GRUB */
    while ((uintptr_t)tag < info_end && tag->type != MULTIBOOT2_TAG_END) {
        if (tag->type == MULTIBOOT2_TAG_MMAP) {
            struct multiboot2_tag_mmap *mmap = (struct multiboot2_tag_mmap *)tag;
            uintptr_t entry_ptr = (uintptr_t)mmap->entries;
            uintptr_t mmap_end = (uintptr_t)mmap + mmap->size;

            while (entry_ptr + mmap->entry_size <= mmap_end) {
                struct multiboot2_mmap_entry *entry = (struct multiboot2_mmap_entry *)entry_ptr;
                if (entry->type == MULTIBOOT2_MMAP_AVAILABLE) {
                    uintptr_t start = (uintptr_t)entry->addr;
                    uintptr_t end = (uintptr_t)(entry->addr + entry->len);

                    if (end > PMM_MAX_MEMORY) {
                        end = PMM_MAX_MEMORY;
                    }

                    if (end > start) {
                        pmm_mark_range(start, end, 0);
                        if (end > max_addr) {
                            max_addr = (uint32_t)end;
                        }
                    }
                }

                entry_ptr += mmap->entry_size;
            }
        }

        tag = (struct multiboot2_tag *)(((uintptr_t)tag + tag->size + 7U) & ~(uintptr_t)7U);
    }

    if (max_addr != 0) {
        total_frames = max_addr / PMM_FRAME_SIZE;
    }

    /*
     * Passagem 2: Preserva os módulos do Multiboot2 (como o arquivo TAR do initrd).
     * Deve ser feito após o mmap para impedir que um bloco AVAILABLE libere essas áreas.
     */
    tag = (struct multiboot2_tag *)((uintptr_t)mb_info_addr + 8U);
    while ((uintptr_t)tag < info_end &&
           tag->type != MULTIBOOT2_TAG_END) {
        if (tag->type == MULTIBOOT2_TAG_MODULE) {
            struct multiboot2_tag_module *module =
                (struct multiboot2_tag_module *)tag;

            if (module->mod_end > module->mod_start) {
                pmm_mark_range(
                    (uintptr_t)module->mod_start,
                    (uintptr_t)module->mod_end,
                    1
                );
            }
        }

        tag = (struct multiboot2_tag *)(
            ((uintptr_t)tag + tag->size + 7U) &
            ~(uintptr_t)7U
        );
    }

    /* Protege o primeiro 1MB de memória (Real Mode / IVT / BDA / VGA video) */
    pmm_mark_range(0, 0x100000U, 1);

    /* Protege a imagem ELF do Kernel e o bloco de informações do Multiboot2 */
    pmm_mark_range(kernel_start, kernel_end, 1);
    pmm_mark_range((uintptr_t)mb_info_addr, info_end, 1);
}

/* Aloca o primeiro frame de 4KB livre encontrado na memória física */
uint32_t pmm_alloc_frame(void) {
    if (free_frames == 0) {
        return 0;
    }

    for (uint32_t frame = 0; frame < total_frames; ++frame) {
        if (!test_bit(frame_bitmap, frame)) {
            set_bit(frame_bitmap, frame);
            clear_bit(table_frame_bitmap, frame);
            free_frames--;
            return frame * PMM_FRAME_SIZE;
        }
    }

    return 0;
}

/* Aloca um frame obrigatoriamente abaixo do limite de endereço especificado (limit_addr) */
uint32_t pmm_alloc_frame_below(uint32_t limit_addr) {
    uint32_t max_frame;

    if (free_frames == 0U || limit_addr < PMM_FRAME_SIZE) {
        return 0U;
    }

    max_frame = limit_addr / PMM_FRAME_SIZE;
    if (max_frame > total_frames) {
        max_frame = total_frames;
    }

    for (uint32_t frame = 0U; frame < max_frame; ++frame) {
        if (!test_bit(frame_bitmap, frame)) {
            set_bit(frame_bitmap, frame);
            clear_bit(table_frame_bitmap, frame);
            free_frames--;
            return frame * PMM_FRAME_SIZE;
        }
    }

    return 0U;
}

/* Aloca um frame obrigatoriamente acima do endereço físico mínimo fornecido */
uint32_t pmm_alloc_frame_above(uint32_t min_addr) {
    uint32_t start_frame;

    if (free_frames == 0U) {
        return 0U;
    }

    start_frame = min_addr / PMM_FRAME_SIZE;
    if ((min_addr % PMM_FRAME_SIZE) != 0U) {
        start_frame++;
    }

    if (start_frame >= total_frames) {
        return 0U;
    }

    for (uint32_t frame = start_frame; frame < total_frames; ++frame) {
        if (!test_bit(frame_bitmap, frame)) {
            set_bit(frame_bitmap, frame);
            clear_bit(table_frame_bitmap, frame);
            free_frames--;
            return frame * PMM_FRAME_SIZE;
        }
    }

    return 0U;
}

/* Devolve um frame físico previamente alocado ao pool de páginas livres */
void pmm_free_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / PMM_FRAME_SIZE;

    if (frame >= total_frames) {
        return;
    }

    if (test_bit(frame_bitmap, frame)) {
        clear_bit(frame_bitmap, frame);
        clear_bit(table_frame_bitmap, frame);
        free_frames++;
    }
}

/* Aloca um frame exclusivo para tabelas de paginação (Page Directories / Tables) abaixo de 12MB */
uint32_t pmm_alloc_table_frame(void) {
    uint32_t frame_addr = pmm_alloc_frame_below(PMM_TABLE_FRAME_LIMIT);
    uint32_t frame;

    if (frame_addr == 0) {
        return 0;
    }

    frame = frame_addr / PMM_FRAME_SIZE;
    set_bit(table_frame_bitmap, frame);
    return frame_addr;
}

/* Libera um frame que foi alocado para estruturas de tabelas de páginas */
void pmm_free_table_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / PMM_FRAME_SIZE;

    if (frame >= total_frames) {
        return;
    }

    clear_bit(table_frame_bitmap, frame);
    pmm_free_frame(frame_addr);
}

/* Verifica se determinado frame físico está marcado como estrutura de tabela de páginas */
uint8_t pmm_is_table_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / PMM_FRAME_SIZE;

    if (frame >= total_frames) {
        return 0;
    }

    return (uint8_t)(test_bit(table_frame_bitmap, frame) != 0U);
}

/* Retorna a quantidade total de frames de 4KB livres na memória física */
uint32_t pmm_free_frame_count(void) {
    return free_frames;
}

/* Retorna a quantidade total de frames de 4KB gerenciados pelo PMM */
uint32_t pmm_total_frame_count(void) {
    return total_frames;
}