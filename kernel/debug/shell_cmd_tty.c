#include "shell_internal.h"
#include <kernel/vga.h>
#include <kernel/klog.h>
#include <kernel/vfs.h>
#include <kernel/tty.h>
#include <kernel/fd.h>
#include <kernel/sched.h>
#include <kernel/task.h>

void shell_cmd_ttytest(void) {
    fs_node_t *tty = vfs_resolve("/dev/tty1");
    static const uint8_t sample[] = {'t', 't', 'y', '1', '\n'};
    static const uint8_t output[] = "[tty1] output path OK\n";
    uint8_t readback[sizeof(sample)];
    uint8_t ok = 1U;

    if (tty == 0) {
        klog_warn("ttytest: /dev/tty1 unavailable");
        return;
    }

    tty1_set_mode(TTY_MODE_RAW);
    tty1_set_echo(0);
    tty1_flush_input();

    for (uint32_t i = 0U; i < sizeof(sample); ++i) {
        tty1_receive_char((char)sample[i]);
    }

    if (tty1_pending() != sizeof(sample)) {
        ok = 0U;
    }

    if (ok && read_fs(tty, 0U, sizeof(readback), readback) != sizeof(readback)) {
        ok = 0U;
    }

    if (ok) {
        for (uint32_t i = 0U; i < sizeof(sample); ++i) {
            if (readback[i] != sample[i]) {
                ok = 0U;
                break;
            }
        }
    }

    if (tty1_pending() != 0U) {
        ok = 0U;
    }

    if (write_fs(tty, 0U, (uint32_t)sizeof(output) - 1U, output) != (uint32_t)sizeof(output) - 1U) {
        ok = 0U;
    }

    vga_puts("ttytest: result=");
    vga_puts(ok ? "TTY1 OK" : "FAILED");
    vga_puts(" dropped=");
    vga_putdec(tty1_dropped());
    vga_puts("\n");
}

void shell_cmd_stdiotest(void) {
    process_t *process = sched_current_process_ptr();
    fs_node_t *tty = vfs_resolve("/dev/tty1");
    static const uint8_t stdin_sample[] = {'O', 'K'};
    static const uint8_t stdout_sample[] = "[stdio] fd 1 -> /dev/tty1 OK\n";
    uint8_t stdin_readback[sizeof(stdin_sample)];
    uint8_t ok = 1U;

    if (process == 0 || tty == 0) {
        ok = 0U;
    }

    if (ok) {
        fd_entry_t *stdin_entry = &process->fds.entries[0];
        fd_entry_t *stdout_entry = &process->fds.entries[1];
        fd_entry_t *stderr_entry = &process->fds.entries[2];

        if (stdin_entry->kind != FD_KIND_VFS ||
            stdout_entry->kind != FD_KIND_VFS ||
            stderr_entry->kind != FD_KIND_VFS ||
            stdin_entry->node != tty ||
            stdout_entry->node != tty ||
            stderr_entry->node != tty ||
            stdin_entry->access != FD_ACCESS_READ ||
            stdout_entry->access != FD_ACCESS_WRITE ||
            stderr_entry->access != FD_ACCESS_WRITE) {
            ok = 0U;
        }
    }

    if (ok) {
        tty1_set_mode(TTY_MODE_RAW);
        tty1_set_echo(0);
        tty1_flush_input();
        tty1_receive_char((char)stdin_sample[0]);
        tty1_receive_char((char)stdin_sample[1]);

        if (fd_read(&process->fds, 0U, stdin_readback, sizeof(stdin_readback)) != (int32_t)sizeof(stdin_readback) ||
            stdin_readback[0] != stdin_sample[0] ||
            stdin_readback[1] != stdin_sample[1]) {
            ok = 0U;
        }
    }

    if (ok && fd_write(&process->fds, 1U, stdout_sample, (uint32_t)sizeof(stdout_sample) - 1U) != (int32_t)((uint32_t)sizeof(stdout_sample) - 1U)) {
        ok = 0U;
    }

    if (ok) {
        uint32_t new_offset = 0U;

        if (fd_seek(&process->fds, 1U, 0, 0U, &new_offset) != -2) {
            ok = 0U;
        }
    }

    vga_puts("stdiotest: result=");
    vga_puts(ok ? "STDIO->TTY1 OK" : "FAILED");
    vga_puts("\n");
}

void shell_cmd_ttyfocus(const char *arg) {
    if (arg == 0 || *arg == 0) {
        vga_puts("ttyfocus: ");
        vga_puts(tty1_input_focus() == TTY_INPUT_FOCUS_TTY1 ? "tty1" : "shell");
        vga_puts(" fgpid=");
        vga_putdec(tty1_foreground_pid());
        vga_puts(" mode=");
        vga_puts(tty1_mode() == TTY_MODE_CANONICAL ? "canonical" : "raw");
        vga_puts(" echo=");
        vga_puts(tty1_echo_enabled() ? "on" : "off");
        vga_puts(" pending=");
        vga_putdec(tty1_pending());
        vga_puts("\n");
        return;
    }

    if (shell_str_eq(arg, "shell")) {
        tty1_set_input_focus(TTY_INPUT_FOCUS_SHELL);
        vga_puts("ttyfocus: kernel shell\n");
        return;
    }

    if (shell_str_eq(arg, "tty1")) {
        tty1_flush_input();
        tty1_set_input_focus(TTY_INPUT_FOCUS_TTY1);
        vga_puts("ttyfocus: /dev/tty1 owns input (F12 or Ctrl+] returns to kernel shell)\n");
        return;
    }

    klog_warn("usage: ttyfocus [shell|tty1]");
}
