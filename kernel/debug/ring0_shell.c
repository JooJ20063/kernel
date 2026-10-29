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


/* Comando: devtest — Valida leitura/escrita em dispositivos pseudo-caractere (/dev/null, /dev/zero) */


/* Comando: ttytest — Testa buffers de recepção e transmissão no dispositivo /dev/tty1 */


/* Comando: stdiotest — Valida o vínculo dos descritores de arquivo 0, 1 e 2 (stdio) com a TTY */


/* Comando: ttyfocus — Define ou exibe o dono do foco de entrada de teclado (Kernel Shell vs TTY1) */




/* Análise e despacho principal de comandos do Shell em Ring 0 */


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