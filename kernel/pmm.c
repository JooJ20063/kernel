#include <kernel/pmm.h>
#include <kernel/multiboot2.h>

#define PMM_MAX_MEMORY (512U * 1024U * 1024U)
#define PMM_MAX_FRAMES (PMM_MAX_MEMORY / PMM_FRAME_SIZE)

static uint8_t frame_bitmap[PMM_MAX_FRAMES / 8];
static uint32_t total_frames;
static uint32_t free_frames;

static void set_bit(uint32_t frame) {
    frame_bitmap[frame / 8] |= (uint8_t)(1U << (frame % 8));
}

static void clear_bit(uint32_t frame) {
    frame_bitmap[frame / 8] &= (uint8_t)~(1U << (frame % 8));
}

static uint8_t test_bit(uint32_t frame) {
    return (uint8_t)(frame_bitmap[frame / 8] & (1U << (frame % 8)));
}

static void pmm_mark_range(uintptr_t start, uintptr_t end, uint8_t used) {
    uintptr_t aligned_start = start & ~(uintptr_t)(PMM_FRAME_SIZE - 1U);
    uintptr_t aligned_end = (end + PMM_FRAME_SIZE - 1U) & ~(uintptr_t)(PMM_FRAME_SIZE - 1U);

    if (aligned_end > (uintptr_t)(total_frames * PMM_FRAME_SIZE)) {
        aligned_end = (uintptr_t)(total_frames * PMM_FRAME_SIZE);
    }

    for (uintptr_t addr = aligned_start; addr < aligned_end; addr += PMM_FRAME_SIZE) {
        uint32_t frame = (uint32_t)(addr / PMM_FRAME_SIZE);
        if (frame >= total_frames) {
            break;
        }

        if (used) {
            if (!test_bit(frame)) {
                set_bit(frame);
                free_frames--;
            }
        } else {
            if (test_bit(frame)) {
                clear_bit(frame);
                free_frames++;
            }
        }
    }
}

void pmm_init(uint32_t memory_top_bytes) {
    if (memory_top_bytes > PMM_MAX_MEMORY) {
        memory_top_bytes = PMM_MAX_MEMORY;
    }

    total_frames = memory_top_bytes / PMM_FRAME_SIZE;
    free_frames = 0;

    for (uint32_t i = 0; i < sizeof(frame_bitmap); ++i) {
        frame_bitmap[i] = 0xFF;
    }

    if (total_frames == 0) {
        return;
    }

    pmm_mark_range(0x100000U, (uintptr_t)memory_top_bytes, 0);
}

void pmm_init_from_multiboot(uint32_t mb_info_addr, uintptr_t kernel_start, uintptr_t kernel_end) {
    uint32_t total_size = *(uint32_t *)(uintptr_t)mb_info_addr;
    uintptr_t info_end = (uintptr_t)mb_info_addr + (uintptr_t)total_size;
    struct multiboot2_tag *tag = (struct multiboot2_tag *)((uintptr_t)mb_info_addr + 8U);
    uint32_t max_addr = 0;

    pmm_init(PMM_MAX_MEMORY);

    while ((uintptr_t)tag < info_end && tag->type != MULTIBOOT2_TAG_END) {
        if (tag->type == MULTIBOOT2_TAG_MMAP) {
            struct multiboot2_tag_mmap *mmap = (struct multiboot2_tag_mmap *)tag;
            uintptr_t entry_ptr = (uintptr_t)mmap->entries;
            uintptr_t mmap_end = (uintptr_t)mmap + mmap->size;

            while (entry_ptr + mmap->entry_size <= mmap_end) {
                struct multiboot2_mmap_entry *entry = (struct multiboot2_mmap_entry *)entry_ptr;
                if (entry->type == MULTIBOOT2_MMAP_AVAILABLE) {
                    uintptr_t start = (uintptr_t)entry->addr;
                    uintptr_t end = (uintptr_t)(entry->addr + entry->len);

                    if (end > PMM_MAX_MEMORY) {
                        end = PMM_MAX_MEMORY;
                    }

                    if (end > start) {
                        pmm_mark_range(start, end, 0);
                        if (end > max_addr) {
                            max_addr = (uint32_t)end;
                        }
                    }
                }

                entry_ptr += mmap->entry_size;
            }
        }

        tag = (struct multiboot2_tag *)(((uintptr_t)tag + tag->size + 7U) & ~(uintptr_t)7U);
    }

    if (max_addr != 0) {
        total_frames = max_addr / PMM_FRAME_SIZE;
    }

    pmm_mark_range(0, 0x100000U, 1);
    pmm_mark_range(kernel_start, kernel_end, 1);
    pmm_mark_range((uintptr_t)mb_info_addr, info_end, 1);
}

uint32_t pmm_alloc_frame(void) {
    if (free_frames == 0) {
        return 0;
    }

    for (uint32_t frame = 0; frame < total_frames; ++frame) {
        if (!test_bit(frame)) {
            set_bit(frame);
            free_frames--;
            return frame * PMM_FRAME_SIZE;
        }
    }

    return 0;
}

void pmm_free_frame(uint32_t frame_addr) {
    uint32_t frame = frame_addr / PMM_FRAME_SIZE;

    if (frame >= total_frames) {
        return;
    }

    if (test_bit(frame)) {
        clear_bit(frame);
        free_frames++;
    }
}

uint32_t pmm_free_frame_count(void) {
    return free_frames;
}

uint32_t pmm_total_frame_count(void) {
    return total_frames;
}
