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

/* Símbolos e rotinas externas de teste para tarefas de User Mode (Ring 3 em x86) */
#ifndef __x86_64__
extern void user_test_entry(void);
extern void user_fault_test_entry(void);
extern void user_ud_test_entry(void);
extern void user_gp_test_entry(void);
extern void user_aspace_test_entry(void);
extern void user_tty_block_test_entry(void);
extern void user_tty_foreground_test_entry(void);
extern void user_tty_canonical_test_entry(void);
extern uint8_t user_stack_top;
extern void enter_ring3(uint32_t entry, uint32_t user_stack);
#endif

#define SHELL_BUF 128
/* Estado global do buffer de linha de comando e slots de alocação de teste do Heap */
static char line[SHELL_BUF];
static uint32_t line_len;

/* Exibe o prompt de comando no terminal VGA ($ ) com destaque em Ciano */
static void shell_prompt(void) {
    vga_set_color(0x0B, 0x00);
    vga_puts("\n$ ");
    vga_set_color(0x0F, 0x00);
}

/* Restaura o foco e o buffer de entrada do Shell quando devolvido pelo TTY */
void ring0_shell_resume_input(void) {
    line_len = 0U;
    vga_puts("\n[tty] input focus returned to kernel shell");
    shell_prompt();
}





/* Força recursão infinita e estouro de pilha (Stack Overflow) para teste da Guard Page */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winfinite-recursion"
__attribute__((noinline, noreturn, disable_tail_calls))
#else
__attribute__((noinline, noreturn))
#endif
static void force_stack_overflow(uint32_t depth) {
    volatile uint8_t buffer[4096];

    buffer[0] = (uint8_t)depth;
    buffer[4095] = (uint8_t)(depth >> 8);

    asm volatile ("" : : "g"(buffer) : "memory");

    force_stack_overflow(depth + 1);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

/* Comando: pfault — Provoca falhas de página deliberadas para validação do handler #PF */
static void shell_cmd_pfault(const char *arg) {
    if (arg == 0 || *arg == 0){
        klog_warn("usage: pfault [null|readonly|invalid|stack]");
        return;
    }

    if (shell_str_eq(arg, "null")) {
        volatile uint32_t *p = (volatile uint32_t *)0x0;
        *p = 0xDEADBEEFU;
        return;
    }

    if (shell_str_eq(arg, "readonly")) {
        extern uint8_t _rodata_start;
        volatile uint32_t *p = (volatile uint32_t *)&_rodata_start;
        *p = 0xCAFEBABEU;
        return;
    }

    if (shell_str_eq(arg, "invalid")) {
        volatile uint32_t *p = (volatile uint32_t *)0xA000000U;
        *p = 0x12345678U;
        return;
    }

    if (shell_str_eq(arg, "stack")) {
        force_stack_overflow(0);
        return;
    }

    klog_warn("usage: pfault [null|readonly|invalid|stack]");
}

/* Comando: panic — Provoca parada de emergência do Kernel por diferentes causas */
static void shell_cmd_panic(const char *arg) {
    if (arg == 0 || *arg == 0) {
        kernel_panic("panic command invoked", 0);
        return;
    }

    if (shell_str_eq(arg, "int3")) {
        asm volatile ("int3");
        return;
    }

    if (shell_str_eq(arg, "ud2")) {
        asm volatile ("ud2");
        return;
    }

    if (shell_str_eq(arg, "div0")) {
        klog_warn("div0: disabled - use 'panic ud2' or 'panic int3' instead");
        return;
    }

    if (shell_str_eq(arg, "null")) {
        volatile uint32_t *pnull = (volatile uint32_t *)0x0;
        *pnull = 0xDEADBEEFU;
        return;
    }

    if (shell_str_starts(arg, "int ")) {
        int ok;
        uint32_t int_no = shell_parse_u32(arg + 4, &ok);
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

/* Comando: ls — Lista as entradas de um diretório no VFS */


/* Comando: elftest — Inspeciona um executável ELF32 sem carregá-lo na memória */




/* Verifica a integridade dos dados carregados na memória virtual em comparação com o arquivo ELF */


/* Comando: elfloadtest — Executa o ciclo completo de carga, validação de hash/conteúdo e descarga do ELF */


/* Comando: elfrun — Carrega e cria um processo executável em Ring 3 a partir de um binário ELF32 */


/* Comando: cat — Exibe o conteúdo de um arquivo texto do VFS no terminal */


/* Escreve ou redireciona texto para um arquivo especificado no VFS */


/* Comando: touch — Cria um arquivo vazio no VFS */


/* Comando: mkdir — Cria um diretório no VFS */


/* Comando: virt — Converte um endereço virtual informado em endereço físico via VMM */


/* Comando: mapped — Consulta se uma página virtual está mapeada no VMM */


/* Comando: unmap — Remove o mapeamento de uma página virtual no VMM */


/* Comando: kslots — Exibe os slots ativos de monitoramento do Heap */


/* Comando: kmalloc — Realiza alocação de teste no Heap do Kernel associada a um slot */


/* Comando: kfree — Libera um bloco alocado no Heap do Kernel via slot */


/* Comando: krealloc — Redimensiona a alocação de um slot do Heap */


/* Comando: tmptest — Testa operações de cópia/zero em janelas temporárias de mapeamento físico */


/* Comando: mounttest — Testa montagem, navegação e desmontagem de pontos no VFS */
static void shell_cmd_mounttest(void) {
    fs_node_t *source;
    fs_node_t *target;
    fs_node_t *probe;
    fs_node_t *mounted_root;
    fs_node_t *mounted_probe;
    uint32_t baseline_mounts = vfs_mount_count();
    uint8_t ok = 1U;

    source = vfs_resolve("/tmp");
    if (source == 0) {
        klog_warn("mounttest: /tmp unavailable");
        return;
    }

    target = vfs_create("/mnttest", FS_DIRECTORY);
    if (target == 0) {
        klog_warn("mounttest: failed to create /mnttest");
        return;
    }

    probe = vfs_create("/tmp/__mount_probe", FS_FILE | FS_WRITABLE);
    if (probe == 0) {
        (void)vfs_remove("/mnttest");
        klog_warn("mounttest: failed to create probe");
        return;
    }

    if (vfs_mount("/mnttest", source) != 0) {
        (void)vfs_remove("/tmp/__mount_probe");
        (void)vfs_remove("/mnttest");
        klog_warn("mounttest: mount failed");
        return;
    }

    mounted_root = vfs_resolve("/mnttest");
    mounted_probe = vfs_resolve("/mnttest/__mount_probe");

    if (mounted_root != source ||
        mounted_probe != probe ||
        vfs_mount_count() != baseline_mounts + 1U) {
        ok = 0U;
    }

    if (vfs_unmount("/mnttest") != 0) {
        ok = 0U;
    }

    if (vfs_resolve("/mnttest") != target ||
        vfs_resolve("/mnttest/__mount_probe") != 0 ||
        vfs_mount_count() != baseline_mounts) {
        ok = 0U;
    }

    if (vfs_remove("/tmp/__mount_probe") != 0 ||
        vfs_remove("/mnttest") != 0) {
        ok = 0U;
    }

    if (vfs_resolve("/tmp/__mount_probe") != 0 ||
        vfs_resolve("/mnttest") != 0) {
        ok = 0U;
    }

    vga_puts("mounttest: result=");
    vga_puts(ok ? "MOUNT OK" : "FAILED");
    vga_puts("\n");
}

/* Comando: devtest — Valida leitura/escrita em dispositivos pseudo-caractere (/dev/null, /dev/zero) */
static void shell_cmd_devtest(void) {
    fs_node_t *null_node = vfs_resolve("/dev/null");
    fs_node_t *zero_node = vfs_resolve("/dev/zero");
    uint8_t zero_buf[32];
    static const uint8_t payload[] = "cruzeiro";
    uint8_t ok = 1U;

    if (null_node == 0 || zero_node == 0) {
        ok = 0U;
    }

    if (ok && read_fs(null_node, 0U, sizeof(zero_buf), zero_buf) != 0U) {
        ok = 0U;
    }

    if (ok && write_fs(null_node, 0U, (uint32_t)(sizeof(payload) - 1U), payload) != (uint32_t)(sizeof(payload) - 1U)) {
        ok = 0U;
    }

    if (ok) {
        for (uint32_t i = 0U; i < sizeof(zero_buf); ++i) {
            zero_buf[i] = 0xA5U;
        }

        if (read_fs(zero_node, 0U, sizeof(zero_buf), zero_buf) != sizeof(zero_buf)) {
            ok = 0U;
        }
    }

    if (ok) {
        for (uint32_t i = 0U; i < sizeof(zero_buf); ++i) {
            if (zero_buf[i] != 0U) {
                ok = 0U;
                break;
            }
        }
    }

    vga_puts("devtest: result=");
    vga_puts(ok ? "DEVFS OK" : "FAILED");
    vga_puts("\n");
}

/* Comando: ttytest — Testa buffers de recepção e transmissão no dispositivo /dev/tty1 */


/* Comando: stdiotest — Valida o vínculo dos descritores de arquivo 0, 1 e 2 (stdio) com a TTY */


/* Comando: ttyfocus — Define ou exibe o dono do foco de entrada de teclado (Kernel Shell vs TTY1) */


#ifndef __x86_64__
/* Tarefa temporária usada no teste de desbloqueio por entrada na TTY */
static void ttyblock_waker_task(void) {
    task_sleep_ticks(100U);
    tty1_receive_char('Z');
    task_exit();
}
#endif

/* Análise e despacho principal de comandos do Shell em Ring 0 */
static void shell_run_command(const char *cmd) {
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
    } else if (shell_str_eq(cmd, "task")) {
        shell_cmd_task();
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
    } else if (shell_str_eq(cmd, "tmptest")) {
        shell_cmd_tmptest();
    } else if (shell_str_eq(cmd, "mounttest")) {
        shell_cmd_mounttest();
    } else if (shell_str_eq(cmd, "devtest")) {
        shell_cmd_devtest();
    } else if (shell_str_eq(cmd, "ttytest")) {
        shell_cmd_ttytest();
    } else if (shell_str_eq(cmd, "stdiotest")) {
        shell_cmd_stdiotest();
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
    } else if (shell_str_eq(cmd, "kslots")) {
        shell_cmd_kslots();
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
    } else if (shell_str_eq(cmd, "ps")) {
        shell_cmd_ps();
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
    } else if (shell_str_eq(cmd, "schedtest")) {
        shell_cmd_schedtest();
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
    } else if (shell_str_eq(cmd, "ttyfgtest")) {
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
    } else if (shell_str_eq(cmd, "ttycantest")) {
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
    } else if (shell_str_eq(cmd, "ring3fault")) {
        shell_cmd_ring3fault();
    } else if (shell_str_eq(cmd, "ring3ud")) {
        shell_cmd_ring3ud();
    } else if (shell_str_eq(cmd, "ring3gp")) {
        shell_cmd_ring3gp();
    } else if (shell_str_eq(cmd, "ring3as")) {
        shell_cmd_ring3as();
#endif
    } else if (shell_str_eq(cmd, "lastexit")) {
        shell_cmd_lastexit();
    } else if (shell_str_eq(cmd, "waittest")) {
        shell_cmd_waittest();
    } else if (cmd[0] != 0) {
        klog_warn("unknown command");
    }
}

/* Inicializa as variáveis do Shell de diagnósticos e exibe a mensagem pronta */
void ring0_shell_init(void) {
    line_len = 0;

    shell_memory_init();

    klog_info("shell ready (type 'help')");
    shell_prompt();
}

/* Handler de entrada de caractere vindo do teclado/serial para tratamento no Shell */
void ring0_shell_on_key(char c) {
    if (c == '\r') {
        return;
    }

    if (c == '\n') {
        line[line_len] = 0;
        shell_run_command(line);
        line_len = 0;

        if (tty1_input_focus() == TTY_INPUT_FOCUS_SHELL) {
            shell_prompt();
        }

        return;
    }

    if (c == '\b') {
        if (line_len > 0) {
            line_len--;
            uint16_t p = vga_get_cursor_pos();

            if (p > 0) {
                vga_set_serial_mirror(0);
                vga_set_cursor_pos(p - 1);
                vga_putc(' ');
                vga_set_cursor_pos(p - 1);
                vga_set_serial_mirror(1);
            }

            if (serial_is_available()) {
                serial_puts("\b \b");
            }
        }
        return;
    }

    if (line_len < (SHELL_BUF - 1)) {
        line[line_len++] = c;
        vga_putc(c);
    }
}