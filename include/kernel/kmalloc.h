#pragma once
#include <stdint.h>

void kmalloc_init(void);
void *kmalloc(uint32_t size);
void kfree(void *ptr);
void *krealloc(void *ptr, uint32_t size);

uint32_t kmalloc_bytes_used(void);
uint32_t kmalloc_bytes_free(void);
uint32_t kmalloc_bytes_mapped(void);
uint32_t kmalloc_block_count(void);

uint8_t kheap_check(void);
