#include <kernel/shell.h>
#include <kernel/vga.h>
#include <kernel/klog.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/kmalloc.h>
#include <kernel/sched.h>
#include <kernel/panic.h>
#include <arch/x86/irq.h>
#include <arch/x86/regs.h>

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

static uint32_t parse_u32(const char *s, int *ok) {
    uint32_t v = 0;
    *ok = 0;

    if (*s == 0) {
        return 0;
    }

    while (*s) {
        if (*s < '0' || *s > '9') {
            return 0;
        }

        v = (v * 10U) + (uint32_t)(*s - '0');
        s++;
    }

    *ok = 1;
    return v;
}

static void shell_prompt(void) {
    vga_set_color(0x0B, 0x00);
    vga_puts("\n$ ");
    vga_set_color(0x0F, 0x00);
}

static void shell_cmd_panic(const char *arg) {
    if (arg == 0 || *arg == 0) {
        kernel_panic("panic command invoked", 0);
        return;
    }

    if (str_eq(arg, "int3")) {
        asm volatile ("int3");
        return;
    }

    if (str_eq(arg, "ud2")) {
        asm volatile ("ud2");
        return;
    }

    if (str_eq(arg, "div0")) {
        asm volatile (
            "xor %%edx, %%edx\n"
            "xor %%eax, %%eax\n"
            "div %%edx\n"
            :
            :
            : "eax", "edx");
        return;
    }

    if (str_eq(arg, "null")) {
        volatile uint32_t *pnull = (volatile uint32_t *)0x0;
        *pnull = 0xDEADBEEFU;
        return;
    }

    if (str_starts(arg, "int ")) {
        int ok;
        uint32_t int_no = parse_u32(arg + 4, &ok);
        if (ok) {
            registers_t fake = {0};
            fake.int_no = int_no;
            fake.err = 0;
            kernel_panic("manual interrupt panic", &fake);
            return;
        }
    }

    klog_warn("panic usage: panic [int3|ud2|div0|null|int <n>]");
}

static void shell_run_command(const char *cmd) {
    if (str_eq(cmd, "help")) {
        vga_puts("cmds: help clear ticks task pmm vmm wp nullguard kmalloc kheap echo panic\n");
        vga_puts("kmalloc <bytes>: alloc + write-test ring0 heap\n");
        vga_puts("panic modes: panic int3 | panic ud2 | panic div0 | panic null | panic int <n>\n");
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
    } else if (str_eq(cmd, "vmm")) {
        vga_puts("paging=");
        vga_puts(vmm_is_enabled() ? "ON" : "OFF");
        vga_puts(" wp=");
        vga_puts(vmm_wp_is_enabled() ? "ON" : "OFF");
        vga_puts("\n");
    } else if (str_eq(cmd, "wp")) {
        vga_puts("CR0.WP=");
        vga_puts(vmm_wp_is_enabled() ? "ON" : "OFF");
        vga_puts("\n");
    } else if (str_eq(cmd, "nullguard")) {
        vga_puts("null-page guard ativo (0x0 sem mapeamento). Teste com: panic null\n");
    } else if (str_eq(cmd, "kheap")) {
        vga_puts("kheap used=");
        vga_putdec(kmalloc_bytes_used());
        vga_puts(" mapped=");
        vga_putdec(kmalloc_bytes_mapped());
        vga_puts("\n");
    } else if (str_starts(cmd, "kmalloc ")) {
        int ok;
        uint32_t sz = parse_u32(cmd + 8, &ok);

        if (!ok || sz == 0) {
            klog_warn("usage: kmalloc <bytes>");
            return;
        }

        {
            uint32_t *ptr = (uint32_t *)kmalloc(sz);
            if (ptr == 0) {
                klog_warn("kmalloc failed");
                return;
            }

            *ptr = 0xCAFEBABEU;

            vga_puts("kmalloc ok ptr=");
            vga_puthex((uint32_t)(uintptr_t)ptr);
            vga_puts(" test=");
            vga_puthex(*ptr);
            vga_puts(" used=");
            vga_putdec(kmalloc_bytes_used());
            vga_puts("\n");
        }
    } else if (str_starts(cmd, "echo ")) {
        vga_puts(cmd + 5);
        vga_puts("\n");
    } else if (str_eq(cmd, "panic") || str_starts(cmd, "panic ")) {
        shell_cmd_panic(cmd[5] ? cmd + 6 : 0);
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
