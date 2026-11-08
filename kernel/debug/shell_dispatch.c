#include <kernel/ring0_shell.h>
#include "shell_internal.h"
#include <kernel/vga.h>
#include <kernel/klog.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/kmalloc.h>
#include <kernel/sched.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/serial.h>
#include <kernel/tty.h>
#include <kernel/fd.h>
#include <kernel/elf32.h>

/* Inclusões dependentes da arquitetura para manipulação de registradores e interrupções */
#ifdef __x86_64__
#include <arch/x86_64/irq.h>
#include <arch/x86_64/regs.h>
#else
#include <arch/x86/irq.h>
#include <arch/x86/regs.h>
#include <arch/x86/tss.h>
#endif

#include "shell_internal.h"

/* K0.8b.2: exact-match command registry */
typedef void (*shell_exact_handler_t)(void);
typedef struct {
    const char *name;
    shell_exact_handler_t handler;
} shell_exact_command_t;

static const shell_exact_command_t shell_exact_commands[] = {
    { "task", shell_cmd_task },
    { "ps", shell_cmd_ps },
    { "schedtest", shell_cmd_schedtest },
    { "lastexit", shell_cmd_lastexit },
    { "waittest", shell_cmd_waittest },
    { "tmptest", shell_cmd_tmptest },
    { "mounttest", shell_cmd_mounttest },
    { "devtest", shell_cmd_devtest },
    { "ttytest", shell_cmd_ttytest },
    { "stdiotest", shell_cmd_stdiotest },
    { "kslots", shell_cmd_kslots },
#ifndef __x86_64__
    { "ring3test", shell_cmd_ring3test },
    { "ring3fault", shell_cmd_ring3fault },
    { "ring3ud", shell_cmd_ring3ud },
    { "ring3gp", shell_cmd_ring3gp },
    { "ring3as", shell_cmd_ring3as },
    { "ttyblocktest", shell_cmd_ttyblocktest },
    { "ttyfgtest", shell_cmd_ttyfgtest },
    { "ttycantest", shell_cmd_ttycantest },
#endif
};

static int shell_dispatch_exact(const char *cmd) {
    unsigned int i;
    for (i = 0U; i < (unsigned int)(sizeof(shell_exact_commands) / sizeof(shell_exact_commands[0])); ++i) {
        if (shell_str_eq(cmd, shell_exact_commands[i].name)) {
            shell_exact_commands[i].handler();
            return 1;
        }
    }
    return 0;
}

void shell_run_command(const char *cmd) {
    /* Comandos exatos migrados; os demais seguem no despacho legado. */
    if (shell_dispatch_exact(cmd)) {
        return;
    }

    if (shell_str_eq(cmd, "help")) {
        vga_puts("cmds: help clear ticks task ps pmm vmm tmptest mounttest devtest ttytest stdiotest ttyblocktest ttyfgtest ttycantest ttyfocus wp nullguard pfault kmalloc kfree krealloc kslots kheap kheapcheck ls mkdir cat touch echo elftest elfloadtest elfrun panic shutdown arch virt mapped unmap schedtest tss syscalltest ring3test ring3fault ring3ud ring3gp ring3as lastexit waittest\n");
        vga_puts("write: echo <texto> > <arquivo> | cat > <arquivo> <texto>\n");
        vga_puts("panic modes: panic int3 | panic ud2 | panic div0(disabled) | panic null | panic int <n>\n");
        vga_puts("vmm dbg: virt <hex> | mapped <hex> | unmap <hex>\n");
        vga_puts("heap dbg: kmalloc <bytes> | kfree <slot> | krealloc <slot> <bytes> | kslots | kheapcheck\n");
        vga_puts("elf: elftest [path] | elfloadtest [path] | elfrun [path] (default /bin/hello)\n");
    } else if (shell_str_eq(cmd, "arch")) {
        if (sizeof(void*) == 8) {
            vga_puts("architecture: x86_64\n");
        } else {
            vga_puts("architecture: x86_32\n");
        }
    } else if (shell_str_eq(cmd, "clear")) {
        vga_clear();
    } else if (shell_str_eq(cmd, "ticks")) {
        vga_puts("ticks=");
        vga_putdec(irq_timer_ticks());
        vga_puts(" secs=");
        vga_putdec(irq_timer_seconds());
        vga_puts(" hz=");
        vga_putdec(irq_timer_hz());
        vga_puts("\n");
    } else if (shell_str_eq(cmd, "pmm")) {
        vga_puts("frames total=");
        vga_putdec(pmm_total_frame_count());
        vga_puts(" free=");
        vga_putdec(pmm_free_frame_count());
        vga_puts("\n");
    } else if (shell_str_eq(cmd, "vmm")) {
        vga_puts("paging=");
        vga_puts(vmm_is_enabled() ? "ON" : "OFF");
        vga_puts(" wp=");
        vga_puts(vmm_wp_is_enabled() ? "ON" : "OFF");
        vga_puts(" cr3=");
        vga_puthex(vmm_current_cr3());
        vga_puts(" kernel=");
        vga_puthex(vmm_kernel_cr3());
        vga_puts(" spaces=");
        vga_putdec(vmm_address_space_count());
        vga_puts(" temp=");
        vga_puts(vmm_temp_window_is_ready() ? "ON" : "OFF");
        vga_puts("\n");
    } else if (shell_str_eq(cmd, "ttyfocus")) {
        shell_cmd_ttyfocus(0);
    } else if (shell_str_starts(cmd, "ttyfocus ")) {
        shell_cmd_ttyfocus(shell_skip_spaces(cmd + 9));
    } else if (shell_str_eq(cmd, "wp")) {
        vga_puts("CR0.WP=");
        vga_puts(vmm_wp_is_enabled() ? "ON" : "OFF");
        vga_puts("\n");
    } else if (shell_str_eq(cmd, "nullguard")) {
        vga_puts("null-page guard ativo (0x0 sem mapeamento). Teste com: panic null\n");
    } else if (shell_str_eq(cmd, "kheap")) {
        vga_puts("kheap used=");
        vga_putdec(kmalloc_bytes_used());
        vga_puts(" free=");
        vga_putdec(kmalloc_bytes_free());
        vga_puts(" mapped=");
        vga_putdec(kmalloc_bytes_mapped());
        vga_puts(" blocks=");
        vga_putdec(kmalloc_block_count());
        vga_puts("\n");
    } else if (shell_str_starts(cmd, "kmalloc ")) {
        shell_cmd_kmalloc_slot(shell_skip_spaces(cmd + 8));
    } else if (shell_str_starts(cmd, "kfree ")) {
        shell_cmd_kfree_slot(shell_skip_spaces(cmd + 6));
    } else if (shell_str_starts(cmd, "krealloc ")) {
        shell_cmd_krealloc_slot(shell_skip_spaces(cmd + 9));
    } else if (shell_str_eq(cmd, "ls")) {
        shell_cmd_ls("/");
    } else if (shell_str_starts(cmd, "ls ")) {
        shell_cmd_ls(shell_skip_spaces(cmd + 3));
    } else if (shell_str_starts(cmd, "mkdir ")) {
        shell_cmd_mkdir(shell_skip_spaces(cmd + 6));
    } else if (shell_str_starts(cmd, "touch ")) {
        shell_cmd_touch(cmd + 6);
    } else if (shell_str_eq(cmd, "elftest")) {
        shell_cmd_elftest(0);
    } else if (shell_str_starts(cmd, "elftest ")) {
        shell_cmd_elftest(shell_skip_spaces(cmd + 8));
    } else if (shell_str_eq(cmd, "elfloadtest")) {
        shell_cmd_elfloadtest(0);
    } else if (shell_str_starts(cmd, "elfloadtest ")) {
        shell_cmd_elfloadtest(shell_skip_spaces(cmd + 12));
    } else if (shell_str_eq(cmd, "elfrun")) {
        shell_cmd_elfrun(0);
    } else if (shell_str_starts(cmd, "elfrun ")) {
        shell_cmd_elfrun(shell_skip_spaces(cmd + 7));
    } else if (shell_str_starts(cmd, "cat > ")) {
        uint32_t i = 6;
        uint32_t file_start = i;
        char name[128];

        while (cmd[i] != 0 && cmd[i] != ' ') {
            i++;
        }

        shell_str_copy_range(name, sizeof(name), cmd, file_start, i);

        if (cmd[i] == ' ') {
            i++;
        }

        shell_write_text_file(name, cmd + i);
    } else if (shell_str_starts(cmd, "cat ")) {
        shell_cmd_cat(cmd + 4);
    } else if (shell_str_starts(cmd, "echo ")) {
        int sep = shell_str_find(cmd + 5, " > ");

        if (sep >= 0) {
            char text[128];
            char file[128];
            uint32_t base = 5U;
            uint32_t split = base + (uint32_t)sep;
            uint32_t total = shell_str_len(cmd);

            shell_str_copy_range(text, sizeof(text), cmd, base, split);
            shell_str_copy_range(file, sizeof(file), cmd, split + 3U, total);
            shell_write_text_file(file, text);
        } else {
            vga_puts(cmd + 5);
            vga_puts("\n");
        }
    } else if (shell_str_starts(cmd, "virt ")) {
        shell_cmd_virt(shell_skip_spaces(cmd + 5));
    } else if (shell_str_starts(cmd, "mapped ")) {
        shell_cmd_mapped(shell_skip_spaces(cmd + 7));
    } else if (shell_str_starts(cmd, "unmap ")) {
        shell_cmd_unmap(shell_skip_spaces(cmd + 6));
    } else if (shell_str_eq(cmd, "panic") || shell_str_starts(cmd, "panic ")) {
        shell_cmd_panic(cmd[5] ? cmd + 6 : 0);
    } else if (shell_str_eq(cmd, "pfault") || shell_str_starts(cmd, "pfault ")) {
        shell_cmd_pfault(cmd[6] ? shell_skip_spaces(cmd + 7) : 0);
    } else if (shell_str_eq(cmd, "kheapcheck")) {
        vga_puts("kheapcheck=");
        vga_puts(kheap_check() ? "OK" : "CORRUPT");
        vga_puts("\n");
    } else if (shell_str_eq(cmd, "shutdown")) {
        vga_puts("Shutting down...\n");

        /* Envia o comando de desligamento para a porta ACPI do QEMU (0x604) */
        asm volatile (
            "outw %0, %1"
            :
            : "a"((uint16_t)0x2000),
              "Nd"((uint16_t)0x604)
        );

        asm volatile ("cli");

        for (;;) {
            asm volatile ("hlt");
        }
    } else if (shell_str_eq(cmd, "tss")) {
        vga_puts("TR=");
        vga_puthex((uint32_t)tss_get_selector());
        vga_puts("\n");
    } else if (shell_str_eq(cmd, "syscalltest")) {
        uint32_t ret = syscall_test_write();

        vga_puts("return=");
        vga_putdec(ret);
        vga_puts("\n");

#ifndef __x86_64__
    } else if (shell_str_eq(cmd, "ring3test")) {
        shell_cmd_ring3test();
    } else if (shell_str_eq(cmd, "ttyblocktest")) {
        shell_cmd_ttyblocktest();
    } else if (shell_str_eq(cmd, "ttyfgtest")) {
        shell_cmd_ttyfgtest();
    } else if (shell_str_eq(cmd, "ttycantest")) {
        shell_cmd_ttycantest();
    } else if (shell_str_eq(cmd, "ring3fault")) {
        shell_cmd_ring3fault();
    } else if (shell_str_eq(cmd, "ring3ud")) {
        shell_cmd_ring3ud();
    } else if (shell_str_eq(cmd, "ring3gp")) {
        shell_cmd_ring3gp();
    } else if (shell_str_eq(cmd, "ring3as")) {
        shell_cmd_ring3as();
#endif
    } else if (cmd[0] != 0) {
        klog_warn("unknown command");
    }
}
