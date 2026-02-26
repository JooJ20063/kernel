#pragma once

#include <stdint.h>

void kmalloc_init(void);
void *kmalloc(uint32_t size);
uint32_t kmalloc_bytes_used(void);
uint32_t kmalloc_bytes_mapped(void);
