#include <kernel/vmm.h>
#include <stdint.h>

void vmm_init(void) {
    /* 64-bit long mode preconfigured by boot stub; no additional mapping needed for now. */
}

uint8_t vmm_is_enabled(void) {
    return 1;
}

uint8_t vmm_wp_is_enabled(void) {
    return 1;
}

int vmm_map_page(uintptr_t virt_addr, uintptr_t phys_addr, uint32_t flags) {
    /* no-op for basic x86_64 early build (identity maps from bootloader) */
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
    return 1;
}

uintptr_t vmm_translate(uintptr_t virt_addr) {
    return virt_addr;
}
