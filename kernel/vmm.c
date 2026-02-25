#include <kernel/vmm.h>

#define PAGE_PRESENT 0x1U
#define PAGE_RW 0x2U

#define VMM_IDENTITY_MB 16U
#define VMM_TABLE_COUNT (VMM_IDENTITY_MB / 4U)

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_tables[VMM_TABLE_COUNT][1024] __attribute__((aligned(4096)));
static uint8_t vmm_enabled;

static void vmm_map_identity(uint32_t megabytes) {
    uint32_t addr = 0;
    uint32_t tables = megabytes / 4U;

    if (tables > VMM_TABLE_COUNT) {
        tables = VMM_TABLE_COUNT;
    }

    for (uint32_t i = 0; i < 1024; ++i) {
        page_directory[i] = 0;
    }

    for (uint32_t t = 0; t < tables; ++t) {
        for (uint32_t i = 0; i < 1024; ++i) {
            page_tables[t][i] = (addr & 0xFFFFF000U) | PAGE_PRESENT | PAGE_RW;
            addr += 0x1000U;
        }

        page_directory[t] = ((uint32_t)(uintptr_t)page_tables[t] & 0xFFFFF000U) | PAGE_PRESENT | PAGE_RW;
    }
}

void vmm_init(void) {
    uint32_t cr0;

    vmm_map_identity(VMM_IDENTITY_MB);

    asm volatile ("mov %0, %%cr3" : : "r"(page_directory));
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U;
    asm volatile ("mov %0, %%cr0" : : "r"(cr0));

    vmm_enabled = 1;
}

uint8_t vmm_is_enabled(void) {
    return vmm_enabled;
}
