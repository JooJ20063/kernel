#include <kernel/vmm.h>
#include <kernel/pmm.h>

#define PAGE_SIZE           0x1000U
#define PAGE_ENTRIES        1024U
#define PAGE_FRAME_MASK     0xFFFFF000U

#define CR0_PG              0x80000000U
#define CR0_WP              0x00010000U

#define VMM_BOOTSTRAP_MB    16U

static uint32_t page_directory[PAGE_ENTRIES] __attribute__((aligned(4096)));
static uint32_t *page_table_ptrs[PAGE_ENTRIES];
static uint8_t vmm_enabled;

static void mem_zero_u32(uint32_t *dst, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        dst[i] = 0;
    }
}

static void vmm_invlpg(uintptr_t addr) {
    asm volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

static uint32_t vmm_dir_index(uintptr_t addr) {
    return (uint32_t)(addr >> 22);
}

static uint32_t vmm_table_index(uintptr_t addr) {
    return (uint32_t)((addr >> 12) & 0x3FFU);
}

static uint32_t *vmm_get_table(uint32_t dir_idx) {
    if (dir_idx >= PAGE_ENTRIES) {
        return 0;
    }

    return page_table_ptrs[dir_idx];
}

static uint32_t *vmm_ensure_table(uint32_t dir_idx) {
    uint32_t frame;
    uint32_t *table;

    if (dir_idx >= PAGE_ENTRIES) {
        return 0;
    }

    if ((page_directory[dir_idx] & VMM_PAGE_PRESENT) != 0U) {
        return page_table_ptrs[dir_idx];
    }

    frame = pmm_alloc_frame();
    if (frame == 0U) {
        return 0;
    }

    /*
     * IMPORTANTE:
     * Esta implementacao assume que o frame alocado para a page table
     * esta acessivel por identity mapping neste estagio do kernel.
     * Com o PMM atual isso tende a funcionar enquanto houver frames baixos.
     */
    table = (uint32_t *)(uintptr_t)frame;
    mem_zero_u32(table, PAGE_ENTRIES);

    page_directory[dir_idx] = (frame & PAGE_FRAME_MASK) | VMM_PAGE_PRESENT | VMM_PAGE_RW;
    page_table_ptrs[dir_idx] = table;

    return table;
}

static void vmm_map_identity(uint32_t megabytes) {
    uintptr_t limit = (uintptr_t)megabytes * 1024U * 1024U;

    if (megabytes > VMM_BOOTSTRAP_MB) {
        limit = (uintptr_t)VMM_BOOTSTRAP_MB * 1024U * 1024U;
    }

    mem_zero_u32(page_directory, PAGE_ENTRIES);
    mem_zero_u32((uint32_t *)page_table_ptrs, PAGE_ENTRIES);

    /* intencionalmente nao mapeia 0x0 */
    for (uintptr_t addr = PAGE_SIZE; addr < limit; addr += PAGE_SIZE) {
        (void)vmm_map_page(addr, addr, VMM_PAGE_RW);
    }
}

void vmm_init(void) {
    uint32_t cr0;

    vmm_map_identity(VMM_BOOTSTRAP_MB);

    asm volatile ("mov %0, %%cr3" : : "r"(page_directory));
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (CR0_PG | CR0_WP);
    asm volatile ("mov %0, %%cr0" : : "r"(cr0));

    vmm_enabled = 1;
}

uint8_t vmm_is_enabled(void) {
    return vmm_enabled;
}

uint8_t vmm_wp_is_enabled(void) {
    uint32_t cr0;

    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    return (uint8_t)((cr0 & CR0_WP) != 0U);
}

int vmm_map_page(uintptr_t virt_addr, uintptr_t phys_addr, uint32_t flags) {
    uint32_t dir_idx;
    uint32_t table_idx;
    uint32_t page_flags;
    uint32_t *table;

    if ((virt_addr & (PAGE_SIZE - 1U)) != 0U || (phys_addr & (PAGE_SIZE - 1U)) != 0U) {
        return -2;
    }

    dir_idx = vmm_dir_index(virt_addr);
    table_idx = vmm_table_index(virt_addr);
    page_flags = VMM_PAGE_PRESENT | (flags & VMM_PAGE_RW);

    table = vmm_ensure_table(dir_idx);
    if (table == 0) {
        return -1;
    }

    table[table_idx] = ((uint32_t)phys_addr & PAGE_FRAME_MASK) | page_flags;

    if (vmm_enabled) {
        vmm_invlpg(virt_addr);
    }

    return 0;
}

int vmm_unmap_page(uintptr_t virt_addr) {
    uint32_t dir_idx;
    uint32_t table_idx;
    uint32_t *table;
    uint8_t empty = 1;

    if ((virt_addr & (PAGE_SIZE - 1U)) != 0U) {
        return -2;
    }

    dir_idx = vmm_dir_index(virt_addr);
    table_idx = vmm_table_index(virt_addr);

    if ((page_directory[dir_idx] & VMM_PAGE_PRESENT) == 0U) {
        return -1;
    }

    table = vmm_get_table(dir_idx);
    if (table == 0) {
        return -1;
    }

    table[table_idx] = 0U;

    if (vmm_enabled) {
        vmm_invlpg(virt_addr);
    }

    for (uint32_t i = 0; i < PAGE_ENTRIES; ++i) {
        if ((table[i] & VMM_PAGE_PRESENT) != 0U) {
            empty = 0;
            break;
        }
    }

    if (empty) {
        uint32_t table_phys = page_directory[dir_idx] & PAGE_FRAME_MASK;
        page_directory[dir_idx] = 0U;
        page_table_ptrs[dir_idx] = 0;
        pmm_free_frame(table_phys);
    }

    return 0;
}

uint8_t vmm_is_mapped(uintptr_t virt_addr) {
    return (uint8_t)(vmm_translate(virt_addr) != 0U);
}

uintptr_t vmm_translate(uintptr_t virt_addr) {
    uint32_t dir_idx;
    uint32_t table_idx;
    uint32_t *table;
    uint32_t pte;

    dir_idx = vmm_dir_index(virt_addr);
    table_idx = vmm_table_index(virt_addr);

    if ((page_directory[dir_idx] & VMM_PAGE_PRESENT) == 0U) {
        return 0U;
    }

    table = vmm_get_table(dir_idx);
    if (table == 0) {
        return 0U;
    }

    pte = table[table_idx];
    if ((pte & VMM_PAGE_PRESENT) == 0U) {
        return 0U;
    }

    return (uintptr_t)(pte & PAGE_FRAME_MASK) | (virt_addr & 0xFFFU);
}
