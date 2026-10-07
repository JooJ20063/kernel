#include <kernel/tty.h>
#include <kernel/vga.h>

#define TTY1_INPUT_CAPACITY 256U

static fs_node_t tty1_fs_node;
static uint8_t tty1_input[TTY1_INPUT_CAPACITY];
static volatile uint32_t tty1_head;
static volatile uint32_t tty1_tail;
static volatile uint32_t tty1_count;
static volatile uint32_t tty1_drop_count;

static uint32_t tty_irq_save_disable(void) {
    uint32_t flags;

    asm volatile (
        "pushf\n"
        "pop %0\n"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );

    return flags;
}

static void tty_irq_restore(uint32_t flags) {
    if ((flags & (1U << 9)) != 0U) {
        asm volatile ("sti" : : : "memory");
    }
}

static void tty1_noop(fs_node_t *node) {
    (void)node;
}

static void str_copy_limit(char *dst, const char *src, uint32_t limit) {
    uint32_t i = 0U;

    if (limit == 0U) {
        return;
    }

    while (src[i] != 0 && i + 1U < limit) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static uint32_t tty1_read(
    fs_node_t *node,
    uint32_t offset,
    uint32_t size,
    uint8_t *buffer
) {
    uint32_t read_count = 0U;
    uint32_t flags;

    (void)node;
    (void)offset;

    if (buffer == 0 && size != 0U) {
        return 0U;
    }

    flags = tty_irq_save_disable();

    while (read_count < size && tty1_count > 0U) {
        buffer[read_count++] = tty1_input[tty1_tail];
        tty1_tail = (tty1_tail + 1U) % TTY1_INPUT_CAPACITY;
        tty1_count--;
    }

    tty_irq_restore(flags);
    return read_count;
}

static uint32_t tty1_write(
    fs_node_t *node,
    uint32_t offset,
    uint32_t size,
    const uint8_t *buffer
) {
    (void)node;
    (void)offset;

    if (buffer == 0 && size != 0U) {
        return 0U;
    }

    for (uint32_t i = 0U; i < size; ++i) {
        vga_putc((char)buffer[i]);
    }

    return size;
}

void tty1_init(void) {
    for (uint32_t i = 0U; i < (uint32_t)sizeof(tty1_fs_node); ++i) {
        ((uint8_t *)&tty1_fs_node)[i] = 0U;
    }

    str_copy_limit(
        tty1_fs_node.name,
        "tty1",
        sizeof(tty1_fs_node.name)
    );

    tty1_fs_node.flags = FS_FILE | FS_WRITABLE;
    tty1_fs_node.read = tty1_read;
    tty1_fs_node.write = tty1_write;
    tty1_fs_node.open = tty1_noop;
    tty1_fs_node.close = tty1_noop;

    tty1_head = 0U;
    tty1_tail = 0U;
    tty1_count = 0U;
    tty1_drop_count = 0U;
}

void tty1_receive_char(char c) {
    if (tty1_count >= TTY1_INPUT_CAPACITY) {
        tty1_drop_count++;
        return;
    }

    tty1_input[tty1_head] = (uint8_t)c;
    tty1_head = (tty1_head + 1U) % TTY1_INPUT_CAPACITY;
    tty1_count++;
}

void tty1_flush_input(void) {
    uint32_t flags = tty_irq_save_disable();

    tty1_head = 0U;
    tty1_tail = 0U;
    tty1_count = 0U;

    tty_irq_restore(flags);
}

uint32_t tty1_pending(void) {
    uint32_t flags = tty_irq_save_disable();
    uint32_t count = tty1_count;

    tty_irq_restore(flags);
    return count;
}

uint32_t tty1_dropped(void) {
    return tty1_drop_count;
}

fs_node_t *tty1_node(void) {
    return &tty1_fs_node;
}
