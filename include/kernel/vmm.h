#pragma once

#include <stdint.h>

#define VMM_PAGE_PRESENT 0x001U
#define VMM_PAGE_RW      0x002U
#define VMM_PAGE_USER    0x004U

void vmm_init(void);
uint8_t vmm_is_enabled(void);
uint8_t vmm_wp_is_enabled(void);

uint32_t vmm_kernel_cr3(void);
uint32_t vmm_current_cr3(void);
void vmm_switch_address_space(uint32_t cr3);

uint32_t vmm_create_address_space(void);
void vmm_destroy_address_space(uint32_t cr3);
uint32_t vmm_address_space_count(void);

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

int vmm_clone_user_range(
    uint32_t cr3,
    uintptr_t start,
    uintptr_t end,
    uint32_t flags
);

int vmm_map_page(uintptr_t virt_addr, uintptr_t phys_addr, uint32_t flags);
int vmm_unmap_page(uintptr_t virt_addr);
uint8_t vmm_is_mapped(uintptr_t virt_addr);
uintptr_t vmm_translate(uintptr_t virt_addr);
int vmm_get_page_flags(uintptr_t virt_addr, uint32_t *flags_out);
