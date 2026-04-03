#include <arch/x86_64/idt.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/irq.h>
#include <kernel/vga.h>
#include <kernel/pmm.h>
#include <kernel/kmalloc.h>
#include <kernel/panic.h>
#include <kernel/klog.h>
#include <kernel/shell.h>
#include <kernel/ramfs.h>
#include <kernel/syscall.h>
#include <arch/x86_64/regs.h>


static void syscall_handler(registers_t *r) {
    uint64_t syscall_num = r->rax;

    switch (syscall_num) {
        case SYS_WRITE: {
            int fd = (int)r->rbx;
            const char *buf = (const char *)r->rcx;
            uint64_t len = r->rdx;
            if (fd == 1) {
                for (uint64_t i = 0; i < len; i++) {
                    vga_putc(buf[i]);
                }
            }
            r->rax = len;
            break;
        }
        case SYS_EXIT: {
            vga_puts("Exiting via syscall...\n");
            asm volatile ("cli");
            for (;;) {
                asm volatile ("hlt");
            }
            break;
        }
        default:
            r->rax = (uint64_t)-1;
            break;
    }
}

void isr_handler_c(registers_t *r) {
    if (r->int_no == 128) {
        syscall_handler(r);
        return;
    }

    /* For not-yet-implemented exception message we just panic */
    kernel_panic("Unhandled exception", (const registers_t *)r);
}

void kernel_main(void) {
    vga_clear();
    vga_puts("=== Kernel 64 Long Mode ===\n");
    vga_puts("Welcome to x86_64 kernel!\n");
    vga_puts("arch: x86_64\n");
    vga_puts("Commands: help, clear, arch, shutdown\n\n");

    klog_info("booting kernel64");

    idt_init();
    idt_install_isrs();
    idt_install_irqs();

    pic_remap(0x20, 0x28);
    pic_mask_all();
    pic_unmask_irq(0); /* timer */
    pic_unmask_irq(1); /* keyboard */

    irq_init(100, 25);

    /* Basic memory management initialization using fixed size arena for 64-bit early boot */
    pmm_init(64 * 1024 * 1024U); /* 64 MB physical frames managed */

    kmalloc_init();
    init_ramfs(0, 0);

    klog_info("interrupts configured");
    vga_puts("kernel64> ");

    asm volatile ("sti");

    shell_init();

    for (;;) {
        asm volatile ("hlt");
    }
}
