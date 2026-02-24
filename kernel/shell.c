#include <kernel/shell.h>
#include <kernel/vga.h>
#include <kernel/klog.h>
#include <kernel/pmm.h>
#include <kernel/sched.h>
#include <kernel/panic.h>
#include <arch/x86/irq.h>

#define SHELL_BUF 128

static char line[SHELL_BUF];
static uint32_t line_len;

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static int str_starts(const char *a, const char *prefix) {
    while (*prefix) {
        if (*a++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

static void shell_prompt(void) {
    vga_set_color(0x0B, 0x00);
    vga_puts("\n$ ");
    vga_set_color(0x0F, 0x00);
}

static void shell_run_command(const char *cmd) {
    if (str_eq(cmd, "help")) {
        vga_puts("cmds: help clear ticks task pmm echo panic\n");
    } else if (str_eq(cmd, "clear")) {
        vga_clear();
    } else if (str_eq(cmd, "ticks")) {
        vga_puts("ticks=");
        vga_putdec(irq_timer_ticks());
        vga_puts(" secs=");
        vga_putdec(irq_timer_seconds());
        vga_puts(" hz=");
        vga_putdec(irq_timer_hz());
        vga_puts("\n");
    } else if (str_eq(cmd, "task")) {
        vga_puts("task=");
        vga_putdec(sched_current_task());
        vga_puts(" switches=");
        vga_putdec(sched_switch_count());
        vga_puts("\n");
    } else if (str_eq(cmd, "pmm")) {
        vga_puts("frames total=");
        vga_putdec(pmm_total_frame_count());
        vga_puts(" free=");
        vga_putdec(pmm_free_frame_count());
        vga_puts("\n");
    } else if (str_starts(cmd, "echo ")) {
        vga_puts(cmd + 5);
        vga_puts("\n");
    } else if (str_eq(cmd, "panic")) {
        kernel_panic("panic command invoked", 0);
    } else if (cmd[0] != 0) {
        klog_warn("unknown command");
    }
}

void shell_init(void) {
    line_len = 0;
    klog_info("shell ready (type 'help')");
    shell_prompt();
}

void shell_on_key(char c) {
    if (c == '\r') {
        return;
    }

    if (c == '\n') {
        line[line_len] = 0;
        shell_run_command(line);
        line_len = 0;
        shell_prompt();
        return;
    }

    if (c == '\b') {
        if (line_len > 0) {
            line_len--;
            uint16_t p = vga_get_cursor_pos();
            if (p > 0) {
                vga_set_cursor_pos(p - 1);
                vga_putc(' ');
                vga_set_cursor_pos(p - 1);
            }
        }
        return;
    }

    if (line_len < (SHELL_BUF - 1)) {
        line[line_len++] = c;
        vga_putc(c);
    }
}
