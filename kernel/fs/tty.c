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
static volatile tty_mode_t tty1_line_mode;
static volatile uint8_t tty1_echo;
static volatile uint32_t tty1_ready_lines;
static volatile uint32_t tty1_current_line_len;
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

static void tty1_reset_input_locked(void) {
    tty1_head = 0U;
    tty1_tail = 0U;
    tty1_count = 0U;
    tty1_ready_lines = 0U;
    tty1_current_line_len = 0U;
}

static void tty1_echo_erase(void) {
    /*
     * "\b \b" works for both VGA and the mirrored serial console once
     * vga_putc() gives backspace cursor semantics.
     */
    vga_puts("\b \b");
}

static int tty1_input_ready(void *ctx) {
    const uint32_t *pid = (const uint32_t *)ctx;

    if (pid == 0) {
        return 0;
    }

    if (tty1_foreground != *pid) {
        return 0;
    }

    if (tty1_line_mode == TTY_MODE_CANONICAL) {
        return tty1_ready_lines > 0U;
    }

    return tty1_count > 0U;
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
                if (tty1_line_mode == TTY_MODE_CANONICAL) {
                    /*
                     * Canonical readers see data only after a complete line
                     * exists. A short user buffer may split that line across
                     * reads; ready_lines remains non-zero until its newline
                     * is actually consumed.
                     */
                    if (tty1_ready_lines > 0U) {
                        while (read_count < size && tty1_count > 0U) {
                            uint8_t c = tty1_input[tty1_tail];

                            tty1_tail =
                                (tty1_tail + 1U) % TTY1_INPUT_CAPACITY;
                            tty1_count--;
                            buffer[read_count++] = c;

                            if (c == (uint8_t)'\n') {
                                tty1_ready_lines--;
                                break;
                            }
                        }
                    }
                } else {
                    while (read_count < size && tty1_count > 0U) {
                        buffer[read_count++] = tty1_input[tty1_tail];
                        tty1_tail =
                            (tty1_tail + 1U) % TTY1_INPUT_CAPACITY;
                        tty1_count--;
                    }
                }
            }

            tty_irq_restore(flags);

            if (read_count > 0U) {
                return read_count;
            }

            /*
             * Recheck foreground ownership and mode-specific readiness
             * atomically with queue insertion. This preserves the existing
             * lost-wakeup guarantee.
             */
            if (task_wait_until(
                    &tty1_read_waiters,
                    tty1_input_ready,
                    &caller_pid) != 0) {
                /*
                 * The bootstrap/idle context cannot sleep. Preserve the
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

    tty1_reset_input_locked();
    tty1_drop_count = 0U;
    tty1_focus = TTY_INPUT_FOCUS_SHELL;
    tty1_foreground = 0U;
    tty1_line_mode = TTY_MODE_RAW;
    tty1_echo = 0U;
    wait_queue_init(&tty1_read_waiters);
}

void tty1_receive_char(char c) {
    uint32_t flags = tty_irq_save_disable();

    if (c == '\r') {
        c = '\n';
    }

    if (tty1_line_mode == TTY_MODE_CANONICAL) {
        if (c == '\b') {
            if (tty1_current_line_len > 0U) {
                tty1_head =
                    (tty1_head + TTY1_INPUT_CAPACITY - 1U) %
                    TTY1_INPUT_CAPACITY;
                tty1_count--;
                tty1_current_line_len--;

                if (tty1_echo != 0U) {
                    tty1_echo_erase();
                }
            }

            tty_irq_restore(flags);
            return;
        }

        /*
         * Keep one slot available for the newline that commits the current
         * line. Without that reserve a full partial line could never become
         * readable.
         */
        if (c != '\n' &&
            tty1_count >= (TTY1_INPUT_CAPACITY - 1U)) {
            tty1_drop_count++;
            tty_irq_restore(flags);
            return;
        }

        if (tty1_count >= TTY1_INPUT_CAPACITY) {
            tty1_drop_count++;
            tty_irq_restore(flags);
            return;
        }

        tty1_input[tty1_head] = (uint8_t)c;
        tty1_head = (tty1_head + 1U) % TTY1_INPUT_CAPACITY;
        tty1_count++;

        if (c == '\n') {
            tty1_ready_lines++;
            tty1_current_line_len = 0U;

            if (tty1_echo != 0U) {
                vga_putc('\n');
            }

            /*
             * A canonical read becomes ready only at line commit.
             */
            wait_queue_wake_all(&tty1_read_waiters);
        } else {
            tty1_current_line_len++;

            if (tty1_echo != 0U) {
                vga_putc(c);
            }
        }

        tty_irq_restore(flags);
        return;
    }

    if (tty1_count >= TTY1_INPUT_CAPACITY) {
        tty1_drop_count++;
        tty_irq_restore(flags);
        return;
    }

    tty1_input[tty1_head] = (uint8_t)c;
    tty1_head = (tty1_head + 1U) % TTY1_INPUT_CAPACITY;
    tty1_count++;

    if (tty1_echo != 0U) {
        vga_putc(c);
    }

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

    tty1_reset_input_locked();

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

void tty1_set_mode(tty_mode_t mode) {
    uint32_t flags = tty_irq_save_disable();

    tty1_line_mode =
        (mode == TTY_MODE_CANONICAL)
            ? TTY_MODE_CANONICAL
            : TTY_MODE_RAW;

    /*
     * Buffered bytes have different readiness semantics in each mode.
     * Drop them at a mode boundary rather than reinterpret stale input.
     */
    tty1_reset_input_locked();
    wait_queue_wake_all(&tty1_read_waiters);

    tty_irq_restore(flags);
}

tty_mode_t tty1_mode(void) {
    return tty1_line_mode;
}

void tty1_set_echo(int enabled) {
    uint32_t flags = tty_irq_save_disable();

    tty1_echo = enabled ? 1U : 0U;

    tty_irq_restore(flags);
}

int tty1_echo_enabled(void) {
    return tty1_echo != 0U;
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
