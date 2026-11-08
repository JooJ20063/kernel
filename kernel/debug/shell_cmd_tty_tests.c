#include <kernel/sched.h>
#include <kernel/task.h>
#include <kernel/tty.h>
#include <kernel/vga.h>
#include <kernel/klog.h>
#include "shell_internal.h"

#ifndef __x86_64__
extern void user_tty_block_test_entry(void);
extern void user_tty_foreground_test_entry(void);
extern void user_tty_canonical_test_entry(void);
extern uint8_t user_stack_top;

static void ttyblock_waker_task(void) {
    task_sleep_ticks(100U);
    tty1_receive_char('Z');
    task_exit();
}

void shell_cmd_ttyblocktest(void) {
        int reader_pid;
        int waker_pid;

        tty1_set_mode(TTY_MODE_RAW);
        tty1_set_echo(0);
        tty1_flush_input();

        reader_pid = sched_create_user_task(
            "tty-block-reader",
            user_tty_block_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (reader_pid < 0) {
            klog_warn("failed to create tty block reader");
            return;
        }

        waker_pid = sched_create_kernel_task(
            "tty-block-waker",
            ttyblock_waker_task
        );

        if (waker_pid < 0) {
            tty1_receive_char('!');
            klog_warn("failed to create tty block waker");
            return;
        }

        vga_puts("ttyblocktest: reader pid=");
        vga_putdec((uint32_t)reader_pid);
        vga_puts(" waker pid=");
        vga_putdec((uint32_t)waker_pid);
        vga_puts(" foreground=");
        vga_putdec((uint32_t)reader_pid);
        vga_puts("\n");

        tty1_set_foreground_pid((uint32_t)reader_pid);
    }

void shell_cmd_ttyfgtest(void) {
        int reader_pid;

        tty1_set_mode(TTY_MODE_RAW);
        tty1_set_echo(0);
        tty1_flush_input();

        reader_pid = sched_create_user_task(
            "tty-foreground-reader",
            user_tty_foreground_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (reader_pid < 0) {
            klog_warn("failed to create tty foreground reader");
            return;
        }

        vga_puts("ttyfgtest: foreground pid=");
        vga_putdec((uint32_t)reader_pid);
        vga_puts(" - type any key (F12 aborts foreground ownership)\n");

        tty1_set_foreground_pid((uint32_t)reader_pid);
    }

void shell_cmd_ttycantest(void) {
        int reader_pid;

        tty1_set_mode(TTY_MODE_CANONICAL);
        tty1_set_echo(1);
        tty1_flush_input();

        reader_pid = sched_create_user_task(
            "tty-canonical-reader",
            user_tty_canonical_test_entry,
            (uintptr_t)&user_stack_top
        );

        if (reader_pid < 0) {
            tty1_set_mode(TTY_MODE_RAW);
            tty1_set_echo(0);
            klog_warn("failed to create tty canonical reader");
            return;
        }

        vga_puts("ttycantest: type helxo, Backspace twice, then lo and Enter\n");
        vga_puts("ttycantest: expected final line is hello\\n\n");

        tty1_set_foreground_pid((uint32_t)reader_pid);
    }

#endif /* !__x86_64__ */
