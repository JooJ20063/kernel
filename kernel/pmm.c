#include <kernel/pmm.h>

#define PMM_MAX_MEMORY (64U * 1024U * 1024U)
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

void pmm_init(uint32_t memory_top_bytes) {
    if (memory_top_bytes > PMM_MAX_MEMORY) {
        memory_top_bytes = PMM_MAX_MEMORY;
    }

    total_frames = memory_top_bytes / PMM_FRAME_SIZE;
    free_frames = 0;

    for (uint32_t i = 0; i < sizeof(frame_bitmap); ++i) {
        frame_bitmap[i] = 0xFF;
    }

    for (uint32_t frame = 0x100000U / PMM_FRAME_SIZE; frame < total_frames; ++frame) {
        clear_bit(frame);
        free_frames++;
    }
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
