#include <kernel/panic.h>
#include <kernel/vga.h>
#include <kernel/klog.h>

/* Rotinas auxiliares para formatação e impressão individual de registradores com rótulo */
#ifdef __x86_64__
static void panic_reg64(const char *n, uint64_t v) {
    vga_puts(n);
    vga_puts("=");
    vga_puthex64(v);
    vga_puts(" ");
}
#else
static void panic_reg(const char *n, uint32_t v) {
    vga_puts(n);
    vga_puts("=");
    vga_puthex(v);
    vga_puts(" ");
}
#endif

/**
 * Ponto de parada de emergência do Kernel (Kernel Panic).
 * Desativa interrupções de hardware, exibe a tela de erro fatal (texto branco sobre fundo vermelho: 0x0F, 0x04),
 * registra a causa da falha, realiza o dump do estado dos registradores e trava a CPU em loop infinito com 'hlt'.
 */
void kernel_panic(const char *reason, const registers_t *regs) {
    /* Desativa imediatamente a geração de interrupções mascaráveis na CPU */
    asm volatile ("cli");

    /* Configura o modo de exibição de erro fatal no buffer VGA */
    vga_set_color(0x0F, 0x04);
    vga_write_at(0, "*** KERNEL PANIC ***");
    vga_set_cursor_pos(80); // Move o cursor para a segunda linha do modo texto (80 colunas x 25 linhas)

    /* Registra o motivo da falha no log de erros */
    klog_error(reason ? reason : "unknown panic");

    /* Exibe o estado completo do contexto de registradores capturado na falha */
    if (regs != 0) {
#ifdef __x86_64__
        /* Dump de registradores para arquitetura de 64 bits */
        panic_reg64("INT", regs->int_no);
        panic_reg64("ERR", regs->err);
        vga_puts("\n");
        panic_reg64("RIP", regs->rip);
        panic_reg64("CS", regs->cs);
        panic_reg64("RFLAGS", regs->rflags);
        vga_puts("\n");
        panic_reg64("RAX", regs->rax);
        panic_reg64("RBX", regs->rbx);
        panic_reg64("RCX", regs->rcx);
        panic_reg64("RDX", regs->rdx);
        vga_puts("\n");
        panic_reg64("RSI", regs->rsi);
        panic_reg64("RDI", regs->rdi);
        panic_reg64("RBP", regs->rbp);
        panic_reg64("RSP", regs->rsp);
#else
        /* Dump de registradores para arquitetura x86 de 32 bits */
        panic_reg("INT", regs->int_no);
        panic_reg("ERR", regs->err);
        vga_puts("\n");
        panic_reg("EIP", regs->eip);
        panic_reg("CS", regs->cs);
        panic_reg("EFLAGS", regs->eflags);
        vga_puts("\n");
        panic_reg("EAX", regs->eax);
        panic_reg("EBX", regs->ebx);
        panic_reg("ECX", regs->ecx);
        panic_reg("EDX", regs->edx);
        vga_puts("\n");
        panic_reg("ESI", regs->esi);
        panic_reg("EDI", regs->edi);
        panic_reg("EBP", regs->ebp);
        panic_reg("ESP", regs->esp);
#endif
    }

    /* Loop de travamento definitivo da CPU */
    for (;;) {
        asm volatile ("hlt");
    }
}