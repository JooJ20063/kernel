#include <arch/x86/idt.h>
#include <arch/x86/regs.h>
#include <arch/x86/pic.h>
#include <arch/x86/irq.h>
#include <kernel/vga.h>
#include <kernel/pmm.h>

struct exception_info {
    const char *name;
    const char *detail;
};

static const struct exception_info exc[] = {
    {"#DE", "Divide Error"},
    {"#DB", "Debug"},
    {"NMI", "Non-maskable interrupt"},
    {"#BP", "Breakpoint"},
    {"#OF", "Overflow"},
    {"#BR", "BOUND range exceeded"},
    {"#UD", "Invalid opcode"},
    {"#NM", "Device not available"},
    {"#DF", "Double fault"},
    {"CSO", "Coprocessor segment overrun"},
    {"#TS", "Invalid TSS"},
    {"#NP", "Segment not present"},
    {"#SS", "Stack-segment fault"},
    {"#GP", "General protection fault"},
    {"#PF", "Page fault"},
    {"RES", "Reserved"},
    {"#MF", "x87 floating-point"},
    {"#AC", "Alignment check"},
    {"#MC", "Machine check"},
    {"#XM", "SIMD floating-point"},
    {"#VE", "Virtualization"},
    {"#CP", "Control protection"},
    {"22", "Reserved"},
    {"23", "Reserved"},
    {"24", "Reserved"},
    {"25", "Reserved"},
    {"26", "Reserved"},
    {"27", "Reserved"},
    {"28", "Hypervisor injection"},
    {"29", "VMM communication"},
    {"30", "Security exception"},
    {"31", "Reserved"}
};

static void dump_reg_line(const char *name, uint32_t value) {
    vga_puts(name);
    vga_puts("=");
    vga_puthex(value);
    vga_puts(" ");
}

void isr_handler_c(struct regs* r){
    vga_set_color(0x0F, 0x04);
    vga_write_at(80, "EXCEPTION: ");

    if (r->int_no < 32) {
        vga_puts(exc[r->int_no].name);
        vga_puts(" ");
        vga_puts(exc[r->int_no].detail);
    } else {
        vga_puts("UNKNOWN");
    }

    vga_puts("\nINT=");
    vga_puthex(r->int_no);
    vga_puts(" ERR=");
    vga_puthex(r->err);

    vga_puts("\n");
    dump_reg_line("EIP", r->eip);
    dump_reg_line("CS", r->cs);
    dump_reg_line("EFLAGS", r->eflags);

    vga_puts("\n");
    dump_reg_line("EAX", r->eax);
    dump_reg_line("EBX", r->ebx);
    dump_reg_line("ECX", r->ecx);
    dump_reg_line("EDX", r->edx);

    vga_puts("\n");
    dump_reg_line("ESI", r->esi);
    dump_reg_line("EDI", r->edi);
    dump_reg_line("EBP", r->ebp);
    dump_reg_line("ESP", r->esp);

    asm volatile ("cli");
    for (;;);
}

void kernel_main(void) {
   idt_init();
   idt_install_isrs();
   idt_install_irqs();

   pic_remap(0x20, 0x28);
   pic_mask_all();

   pic_unmask_irq(0); /* timer */
   pic_unmask_irq(1); /* keyboard */

   irq_init(100, 25);

   vga_set_color(0x0F, 0x00);
   vga_clear();
   vga_write_at(0, "OK KERNEL !");
   vga_write_at(80, "PMM: init 64MB");

   pmm_init(64U * 1024U * 1024U);
   uint32_t frame = pmm_alloc_frame();
   vga_write_at(160, "PMM free frames: ");
   vga_putdec(pmm_free_frame_count());
   vga_puts(" alloc=");
   vga_puthex(frame);

   vga_write_at(80 * 23, "KEY: ");
   vga_write_at(80 * 24, "TIMER: 0s");

   asm volatile ("sti");

   for(;;);
}
