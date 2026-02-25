#include <kernel/vmm.h>

#define PAGE_SIZE 0x1000U
#define CR0_PG 0x80000000U
#define CR0_WP 0x00010000U

#define VMM_IDENTITY_MB 16U
#define VMM_TABLE_COUNT (VMM_IDENTITY_MB / 4U)

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_tables[VMM_TABLE_COUNT][1024] __attribute__((aligned(4096)));
static uint8_t vmm_enabled;

int vmm_map_page(uintptr_t virt_addr, uintptr_t phys_addr, uint32_t flags) {
    uint32_t dir_idx = (uint32_t)(virt_addr >> 22);
    uint32_t table_idx = (uint32_t)((virt_addr >> 12) & 0x3FFU);
    uint32_t page_flags = VMM_PAGE_PRESENT | (flags & (VMM_PAGE_RW));

    if (dir_idx >= VMM_TABLE_COUNT) {
        return -1;
    }

    page_directory[dir_idx] = ((uint32_t)(uintptr_t)page_tables[dir_idx] & 0xFFFFF000U) |
                              VMM_PAGE_PRESENT | VMM_PAGE_RW;
    page_tables[dir_idx][table_idx] = ((uint32_t)phys_addr & 0xFFFFF000U) | page_flags;

    return 0;
}

static void vmm_map_identity(uint32_t megabytes) {
    uintptr_t limit = (uintptr_t)megabytes * 1024U * 1024U;

    if (megabytes > VMM_IDENTITY_MB) {
        limit = (uintptr_t)VMM_IDENTITY_MB * 1024U * 1024U;
    }

    for (uint32_t i = 0; i < 1024; ++i) {
        page_directory[i] = 0;
    }

    for (uint32_t t = 0; t < VMM_TABLE_COUNT; ++t) {
        for (uint32_t i = 0; i < 1024; ++i) {
            page_tables[t][i] = 0;
        }
    }

    /* Intencionalmente NÃO mapeamos 0x0 (null-page guard). */
    for (uintptr_t addr = PAGE_SIZE; addr < limit; addr += PAGE_SIZE) {
        (void)vmm_map_page(addr, addr, VMM_PAGE_RW);
    }
}

void vmm_init(void) {
    uint32_t cr0;

    vmm_map_identity(VMM_IDENTITY_MB);

    asm volatile ("mov %0, %%cr3" : : "r"(page_directory));
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (CR0_PG | CR0_WP);
    asm volatile ("mov %0, %%cr0" : : "r"(cr0));

    vmm_enabled = 1;
}

uint8_t vmm_is_enabled(void) {
    return vmm_enabled;
}
