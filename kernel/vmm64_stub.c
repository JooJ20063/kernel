#include <kernel/vmm.h>

/*
 * Temporary 64-bit compatibility shim.
 *
 * The original kernel/vmm.c is 32-bit specific (CR0/CR3 asm constraints).
 * For the 64-bit bring-up path we expose no-op/identity implementations so
 * that IRQ/IDT/PIC/shell integration can be validated without pulling 32-bit
 * paging code into long mode.
 */

void vmm_init(void) {}

uint8_t vmm_is_enabled(void) {
    return 0;
}

uint8_t vmm_wp_is_enabled(void) {
    return 0;
}

int vmm_map_page(uintptr_t virt_addr, uintptr_t phys_addr, uint32_t flags) {
    (void)virt_addr;
    (void)phys_addr;
    (void)flags;
    return 0;
}

int vmm_unmap_page(uintptr_t virt_addr) {
    (void)virt_addr;
    return 0;
}

uint8_t vmm_is_mapped(uintptr_t virt_addr) {
    (void)virt_addr;
    return 0;
}

uintptr_t vmm_translate(uintptr_t virt_addr) {
    return virt_addr;
}
