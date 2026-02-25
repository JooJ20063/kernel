#include <kernel/kmalloc.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>

#define KMALLOC_ALIGN 16U
#define KMALLOC_PAGE_SIZE 4096U
#define KMALLOC_HEAP_START 0x00C00000U
#define KMALLOC_HEAP_END   0x01000000U

static uintptr_t heap_curr;
static uintptr_t heap_limit;

static uint32_t align_up(uint32_t value, uint32_t align) {
    return (value + align - 1U) & ~(align - 1U);
}

void kmalloc_init(void) {
    heap_curr = KMALLOC_HEAP_START;
    heap_limit = KMALLOC_HEAP_START;
}

void *kmalloc(uint32_t size) {
    uint32_t aligned_size;

    if (size == 0) {
        return 0;
    }

    aligned_size = align_up(size, KMALLOC_ALIGN);

    if (heap_curr + aligned_size > KMALLOC_HEAP_END) {
        return 0;
    }

    while (heap_curr + aligned_size > heap_limit) {
        uint32_t frame = pmm_alloc_frame();

        if (frame == 0) {
            return 0;
        }

        if (vmm_map_page(heap_limit, frame, VMM_PAGE_RW) != 0) {
            pmm_free_frame(frame);
            return 0;
        }

        heap_limit += KMALLOC_PAGE_SIZE;
    }

    {
        uintptr_t out = heap_curr;
        heap_curr += aligned_size;
        return (void *)out;
    }
}

uint32_t kmalloc_bytes_used(void) {
    return (uint32_t)(heap_curr - KMALLOC_HEAP_START);
}

uint32_t kmalloc_bytes_mapped(void) {
    return (uint32_t)(heap_limit - KMALLOC_HEAP_START);
}
