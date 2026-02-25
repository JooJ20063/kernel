#include <kernel/panic.h>
#include <kernel/vga.h>
#include <kernel/klog.h>

static void panic_reg(const char *n, uint32_t v) {
    vga_puts(n);
    vga_puts("=");
    vga_puthex(v);
    vga_puts(" ");
}

void kernel_panic(const char *reason, const registers_t *regs) {
    asm volatile ("cli");

    vga_set_color(0x0F, 0x04);
    vga_write_at(0, "*** KERNEL PANIC ***");
    vga_set_cursor_pos(80);

    klog_error(reason ? reason : "unknown panic");

    if (regs != 0) {
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
    }

    for (;;) {
        asm volatile ("hlt");
    }
}
