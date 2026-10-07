#include <arch/x86/tss.h>

extern uint64_t gdt_tss;
static tss_entry_t tss;

static void mem_zero(void *ptr, uint32_t size) {
    uint8_t *p = (uint8_t *)ptr;

    for (uint32_t i = 0; i < size; i++) {
        p[i] = 0;
    }
}

static void tss_install_descriptor(void) {
    uint32_t base = (uint32_t)(uintptr_t)&tss;
    uint32_t limit = sizeof(tss) - 1U;
    uint64_t desc = 0;

    desc |= (uint64_t)(limit & 0xFFFFU);
    desc |= (uint64_t)(base & 0xFFFFU) << 16;
    desc |= (uint64_t)((base >> 16) & 0xFFU) << 32;

    /* Present=1, DPL=0, type=0x9 = Available 32-bit TSS */
    desc |= (uint64_t)0x89U << 40;

    desc |= (uint64_t)((limit >> 16) & 0x0FU) << 48;
    desc |= (uint64_t)((base >> 24) & 0xFFU) << 56;

    gdt_tss = desc;
}

uint16_t tss_get_selector(void) {
    uint16_t selector;

    asm volatile (
        "str %0"
        : "=r"(selector)
    );

    return selector;
}

void tss_set_kernel_stack(uint32_t stack_top) {
    tss.esp0 = stack_top;
}

void tss_init(void) {
    mem_zero(&tss, sizeof(tss_entry_t));

    tss.ss0 = 0x10;
    tss.esp0 = 0;
    
    tss.iomap_base = sizeof(tss_entry_t);

    tss_install_descriptor();

    asm volatile(
        "mov $0x28, %%ax\n"
        "ltr %%ax\n"
        :
        :
        : "ax", "memory"
    );
}

