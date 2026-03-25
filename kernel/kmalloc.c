#include <kernel/kmalloc.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/klog.h>

#define KMALLOC_ALIGN        16U
#define KMALLOC_PAGE_SIZE    4096U
#define KMALLOC_HEAP_START   0x00C00000U
#define KMALLOC_HEAP_END     0x01000000U
#define KMALLOC_MIN_SPLIT    32U

#define KMALLOC_MAGIC        0x4B484541U
#define KMALLOC_POISON_ALLOC 0xCDU
#define KMALLOC_POISON_FREE  0xDDU

typedef struct kmalloc_block {
    uint32_t magic;
    uint32_t size;
    uint8_t free;
    uint8_t _pad[3];
    struct kmalloc_block *next;
    struct kmalloc_block *prev;
} kmalloc_block_t;

static uintptr_t heap_limit;
static kmalloc_block_t *heap_head;

static uint32_t align_up(uint32_t value, uint32_t align) {
    return (value + align - 1U) & ~(align - 1U);
}

static uint8_t *ptr_add(void *ptr, uint32_t off) {
    return (uint8_t *)((uintptr_t)ptr + (uintptr_t)off);
}

static uint32_t block_overhead(void) {
    return (uint32_t)sizeof(kmalloc_block_t);
}

static void mem_copy(uint8_t *dst, const uint8_t *src, uint32_t size) {
    for (uint32_t i = 0; i < size; ++i) dst[i] = src[i];
}

static void mem_fill(uint8_t *dst, uint8_t value, uint32_t size) {
    for (uint32_t i = 0; i < size; ++i) dst[i] = value;
}

static void mem_zero(uint8_t *dst, uint32_t size) {
    mem_fill(dst, 0U, size);
}

static void block_init(kmalloc_block_t *blk, uint32_t size, uint8_t free) {
    blk->magic = KMALLOC_MAGIC;
    blk->size = size;
    blk->free = free;
    blk->_pad[0] = blk->_pad[1] = blk->_pad[2] = 0;
    blk->next = 0;
    blk->prev = 0;
}

static uint8_t block_is_valid(const kmalloc_block_t *blk) {
    uintptr_t addr;

    if (!blk) return 0;

    addr = (uintptr_t)blk;

    if (addr < KMALLOC_HEAP_START || addr + block_overhead() > heap_limit) return 0;
    if (blk->magic != KMALLOC_MAGIC) return 0;
    if (addr + block_overhead() + blk->size > heap_limit) return 0;
    if (!(blk->free == 0 || blk->free == 1)) return 0;

    return 1;
}

static void *block_payload(kmalloc_block_t *blk) {
    return (void *)ptr_add(blk, block_overhead());
}

static kmalloc_block_t *payload_to_block(void *ptr) {
    if (!ptr) return 0;
    return (kmalloc_block_t *)((uintptr_t)ptr - block_overhead());
}

static kmalloc_block_t *find_last_block(void) {
    kmalloc_block_t *cur = heap_head;
    if (!cur) return 0;

    while (cur->next) {
        if (!block_is_valid(cur)) return 0;
        cur = cur->next;
    }

    return block_is_valid(cur) ? cur : 0;
}

static kmalloc_block_t *find_first_fit(uint32_t size) {
    kmalloc_block_t *cur = heap_head;

    while (cur) {
        if (!block_is_valid(cur)) return 0;
        if (cur->free && cur->size >= size) return cur;
        cur = cur->next;
    }

    return 0;
}

static void split_block(kmalloc_block_t *blk, uint32_t wanted_size) {
    if (!blk || !block_is_valid(blk) || blk->size < wanted_size) return;

    uint32_t remain = blk->size - wanted_size;
    if (remain <= block_overhead() + KMALLOC_MIN_SPLIT) return;

    uintptr_t new_addr = (uintptr_t)block_payload(blk) + wanted_size;
    kmalloc_block_t *new_blk = (kmalloc_block_t *)new_addr;

    block_init(new_blk, remain - block_overhead(), 1);

    new_blk->next = blk->next;
    new_blk->prev = blk;

    if (blk->next) blk->next->prev = new_blk;

    blk->next = new_blk;
    blk->size = wanted_size;

    mem_fill((uint8_t *)block_payload(new_blk), KMALLOC_POISON_FREE, new_blk->size);
}

static void coalesce_forward(kmalloc_block_t *blk) {
    while (blk && blk->next && blk->next->free) {
        kmalloc_block_t *next = blk->next;

        if (!block_is_valid(next)) return;

        blk->size += block_overhead() + next->size;
        blk->next = next->next;

        if (next->next) next->next->prev = blk;
    }
}

static kmalloc_block_t *coalesce_around(kmalloc_block_t *blk) {
    if (!blk || !block_is_valid(blk)) return 0;

    coalesce_forward(blk);

    if (blk->prev && blk->prev->free) {
        blk = blk->prev;
        coalesce_forward(blk);
    }

    return blk;
}

static int map_heap_pages(uint32_t bytes_needed) {
    uintptr_t new_limit = heap_limit;
    uintptr_t end_needed = heap_limit + bytes_needed;

    if (end_needed > KMALLOC_HEAP_END) return -1;

    while (new_limit < end_needed) {
        uint32_t frame = pmm_alloc_frame();
        if (!frame) return -1;

        if (vmm_map_page(new_limit, frame, VMM_PAGE_RW) != 0) {
            pmm_free_frame(frame);
            return -1;
        }

        mem_zero((uint8_t *)new_limit, KMALLOC_PAGE_SIZE);
        new_limit += KMALLOC_PAGE_SIZE;
    }

    heap_limit = new_limit;
    return 0;
}

static kmalloc_block_t *grow_heap(uint32_t wanted_size) {
    kmalloc_block_t *last = find_last_block();

    if (last && last->free) {
        if (last->size < wanted_size) {
            uint32_t missing = wanted_size - last->size;
            uint32_t bytes = align_up(missing, KMALLOC_PAGE_SIZE);
            uint32_t old_size = last->size;

            if (map_heap_pages(bytes) != 0) return 0;

            last->size += bytes;
            mem_fill((uint8_t *)block_payload(last) + old_size, KMALLOC_POISON_FREE, bytes);
        }
        return last;
    }

    uintptr_t region_start = heap_limit;

    if (map_heap_pages(align_up(block_overhead() + wanted_size, KMALLOC_PAGE_SIZE)) != 0)
        return 0;

    kmalloc_block_t *new_blk = (kmalloc_block_t *)region_start;

    block_init(new_blk, (heap_limit - region_start) - block_overhead(), 1);
    mem_fill((uint8_t *)block_payload(new_blk), KMALLOC_POISON_FREE, new_blk->size);

    if (!heap_head) {
        heap_head = new_blk;
    } else {
        last->next = new_blk;
        new_blk->prev = last;
    }

    return new_blk;
}

void kmalloc_init(void) {
    heap_limit = KMALLOC_HEAP_START;
    heap_head = 0;
}

void *kmalloc(uint32_t size) {
    if (!size) return 0;

    uint32_t aligned = align_up(size, KMALLOC_ALIGN);
    kmalloc_block_t *blk = find_first_fit(aligned);

    if (!blk) {
        blk = grow_heap(aligned);
        if (!blk) return 0;
    }

    split_block(blk, aligned);
    blk->free = 0;

    mem_fill((uint8_t *)block_payload(blk), KMALLOC_POISON_ALLOC, blk->size);

    return block_payload(blk);
}

void kfree(void *ptr) {
    if (!ptr) return;

    kmalloc_block_t *blk = payload_to_block(ptr);

    if (!block_is_valid(blk)) return;

    if (blk->free) {
        klog_warn("double free detected");
        return;
    }

    blk->free = 1;
    mem_fill((uint8_t *)block_payload(blk), KMALLOC_POISON_FREE, blk->size);

    coalesce_around(blk);
}

void *krealloc(void *ptr, uint32_t size) {
    if (!ptr) return kmalloc(size);
    if (!size) { kfree(ptr); return 0; }

    kmalloc_block_t *blk = payload_to_block(ptr);

    if (!block_is_valid(blk) || blk->free) return 0;

    uint32_t aligned = align_up(size, KMALLOC_ALIGN);

    if (blk->size >= aligned) {
        split_block(blk, aligned);
        return ptr;
    }

    if (blk->next && blk->next->free &&
        (blk->size + block_overhead() + blk->next->size) >= aligned) {
        coalesce_forward(blk);
        split_block(blk, aligned);
        return ptr;
    }

    void *new_ptr = kmalloc(aligned);
    if (!new_ptr) return 0;

    uint32_t copy = blk->size < aligned ? blk->size : aligned;
    mem_copy(new_ptr, ptr, copy);

    kfree(ptr);
    return new_ptr;
}

uint32_t kmalloc_bytes_used(void) {
    uint32_t total = 0;
    for (kmalloc_block_t *cur = heap_head; cur; cur = cur->next)
        if (!cur->free) total += cur->size;
    return total;
}

uint32_t kmalloc_bytes_free(void) {
    uint32_t total = 0;
    for (kmalloc_block_t *cur = heap_head; cur; cur = cur->next)
        if (cur->free) total += cur->size;
    return total;
}

uint32_t kmalloc_bytes_mapped(void) {
    return heap_limit - KMALLOC_HEAP_START;
}

uint32_t kmalloc_block_count(void) {
    uint32_t count = 0;
    for (kmalloc_block_t *cur = heap_head; cur; cur = cur->next)
        count++;
    return count;
}

uint8_t kheap_check(void) {
    kmalloc_block_t *prev = 0;

    for (kmalloc_block_t *cur = heap_head; cur; cur = cur->next) {
        if (!block_is_valid(cur)) return 0;
        if (cur->prev != prev) return 0;
        if (cur->next && (uintptr_t)cur->next <= (uintptr_t)cur) return 0;

        prev = cur;
    }

    return 1;
}
