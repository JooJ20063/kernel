#include <kernel/tty.h>
#include <kernel/vga.h>
#include <kernel/task.h>

#define TTY1_INPUT_CAPACITY 256U

static fs_node_t tty1_fs_node;
static uint8_t tty1_input[TTY1_INPUT_CAPACITY];
static volatile uint32_t tty1_head;
static volatile uint32_t tty1_tail;
static volatile uint32_t tty1_count;
static volatile uint32_t tty1_drop_count;
static volatile tty_input_focus_t tty1_focus;
static volatile uint32_t tty1_foreground;
static wait_queue_t tty1_read_waiters;

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

static int tty1_input_ready(void *ctx) {
    const uint32_t *pid = (const uint32_t *)ctx;

    if (pid == 0) {
        return 0;
    }

    return tty1_foreground == *pid && tty1_count > 0U;
}

static uint32_t tty1_read(
    fs_node_t *node,
    uint32_t offset,
    uint32_t size,
    uint8_t *buffer
) {
    (void)node;
    (void)offset;

    if (buffer == 0 && size != 0U) {
        return 0U;
    }

    if (size == 0U) {
        return 0U;
    }

    {
        uint32_t caller_pid = sched_current_pid();

        for (;;) {
            uint32_t read_count = 0U;
            uint32_t flags = tty_irq_save_disable();

            /*
             * Owning /dev/tty1 as an open file is not enough to consume
             * input. Only the process currently in the TTY foreground may
             * drain the input ring.
             */
            if (tty1_foreground == caller_pid) {
                while (read_count < size && tty1_count > 0U) {
                    buffer[read_count++] = tty1_input[tty1_tail];
                    tty1_tail =
                        (tty1_tail + 1U) % TTY1_INPUT_CAPACITY;
                    tty1_count--;
                }
            }

            tty_irq_restore(flags);

            if (read_count > 0U) {
                return read_count;
            }

            /*
             * Recheck foreground ownership and data availability atomically
             * with queue insertion. This preserves the lost-wakeup guarantee
             * while also keeping background readers asleep.
             */
            if (task_wait_until(
                    &tty1_read_waiters,
                    tty1_input_ready,
                    &caller_pid) != 0) {
                /*
                 * The bootstrap/idle context cannot sleep. Preserve the old
                 * non-blocking behavior for that context only.
                 */
                return 0U;
            }
        }
    }
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
    tty1_focus = TTY_INPUT_FOCUS_SHELL;
    tty1_foreground = 0U;
    wait_queue_init(&tty1_read_waiters);
}

void tty1_receive_char(char c) {
    uint32_t flags = tty_irq_save_disable();

    if (tty1_count >= TTY1_INPUT_CAPACITY) {
        tty1_drop_count++;
        tty_irq_restore(flags);
        return;
    }

    tty1_input[tty1_head] = (uint8_t)c;
    tty1_head = (tty1_head + 1U) % TTY1_INPUT_CAPACITY;
    tty1_count++;

    /*
     * Foreground filtering means the first waiter is not necessarily the
     * process allowed to consume input. Wake all readers so the foreground
     * process can observe readiness while background readers re-sleep.
     */
    wait_queue_wake_all(&tty1_read_waiters);
    tty_irq_restore(flags);
}

void tty1_flush_input(void) {
    uint32_t flags = tty_irq_save_disable();

    tty1_head = 0U;
    tty1_tail = 0U;
    tty1_count = 0U;

    tty_irq_restore(flags);
}

void tty1_set_input_focus(tty_input_focus_t focus) {
    uint32_t flags = tty_irq_save_disable();

    if (focus == TTY_INPUT_FOCUS_TTY1) {
        tty1_focus = TTY_INPUT_FOCUS_TTY1;
    } else {
        /*
         * Returning input to the Ring 0 shell is an explicit ownership
         * override. No userspace process remains the TTY foreground owner.
         */
        tty1_focus = TTY_INPUT_FOCUS_SHELL;
        tty1_foreground = 0U;
        wait_queue_wake_all(&tty1_read_waiters);
    }

    tty_irq_restore(flags);
}

tty_input_focus_t tty1_input_focus(void) {
    return tty1_focus;
}

void tty1_set_foreground_pid(uint32_t pid) {
    uint32_t flags = tty_irq_save_disable();

    tty1_foreground = pid;

    if (pid != 0U) {
        tty1_focus = TTY_INPUT_FOCUS_TTY1;
    }

    /*
     * A process blocked because it was in the background may now be the
     * foreground owner. Let all readers re-evaluate their wait condition.
     */
    wait_queue_wake_all(&tty1_read_waiters);
    tty_irq_restore(flags);
}

uint32_t tty1_foreground_pid(void) {
    uint32_t flags = tty_irq_save_disable();
    uint32_t pid = tty1_foreground;

    tty_irq_restore(flags);
    return pid;
}

int tty1_release_foreground(uint32_t pid) {
    uint32_t flags;
    int released = 0;

    if (pid == 0U) {
        return 0;
    }

    flags = tty_irq_save_disable();

    if (tty1_foreground == pid) {
        tty1_foreground = 0U;
        tty1_focus = TTY_INPUT_FOCUS_SHELL;
        wait_queue_wake_all(&tty1_read_waiters);
        released = 1;
    }

    tty_irq_restore(flags);
    return released;
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
