#pragma once

#include <stdint.h>

#define VMM_PAGE_PRESENT 0x001U
#define VMM_PAGE_RW      0x002U

void vmm_init(void);
uint8_t vmm_is_enabled(void);
int vmm_map_page(uintptr_t virt_addr, uintptr_t phys_addr, uint32_t flags);
