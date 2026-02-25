#pragma once

#include <stdint.h>

#define MULTIBOOT2_TAG_END 0
#define MULTIBOOT2_TAG_MMAP 6

#define MULTIBOOT2_MMAP_AVAILABLE 1

struct multiboot2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct multiboot2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} __attribute__((packed));

struct multiboot2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot2_mmap_entry entries[];
} __attribute__((packed));
