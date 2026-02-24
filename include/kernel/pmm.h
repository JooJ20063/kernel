#pragma once

#include <stdint.h>

#define PMM_FRAME_SIZE 4096U

void pmm_init(uint32_t memory_top_bytes);
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t frame_addr);
uint32_t pmm_free_frame_count(void);
uint32_t pmm_total_frame_count(void);
