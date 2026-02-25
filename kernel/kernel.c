#include <arch/x86/idt.h>
#include <arch/x86/pic.h>
#include <arch/x86/irq.h>
#include <kernel/vga.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/panic.h>
#include <kernel/klog.h>
#include <kernel/shell.h>

struct exception_info {
    const char *name;
    const char *detail;
};

extern uint8_t _kernel_start;
extern uint8_t _kernel_end;

static const struct exception_info exc[] = {
    {"#DE", "Divide Error"}, {"#DB", "Debug"}, {"NMI", "Non-maskable interrupt"},
    {"#BP", "Breakpoint"}, {"#OF", "Overflow"}, {"#BR", "BOUND range exceeded"},
    {"#UD", "Invalid opcode"}, {"#NM", "Device not available"}, {"#DF", "Double fault"},
    {"CSO", "Coprocessor segment overrun"}, {"#TS", "Invalid TSS"}, {"#NP", "Segment not present"},
    {"#SS", "Stack-segment fault"}, {"#GP", "General protection fault"}, {"#PF", "Page fault"},
    {"RES", "Reserved"}, {"#MF", "x87 floating-point"}, {"#AC", "Alignment check"},
    {"#MC", "Machine check"}, {"#XM", "SIMD floating-point"}, {"#VE", "Virtualization"},
    {"#CP", "Control protection"}, {"22", "Reserved"}, {"23", "Reserved"},
    {"24", "Reserved"}, {"25", "Reserved"}, {"26", "Reserved"}, {"27", "Reserved"},
    {"28", "Hypervisor injection"}, {"29", "VMM communication"}, {"30", "Security exception"},
    {"31", "Reserved"}
};

void isr_handler_c(registers_t *r){
    const char *reason = "Unhandled exception";

    if (r->int_no < 32) {
        reason = exc[r->int_no].detail;
    }

    kernel_panic(reason, r);
}

void kernel_main(void) {
   vga_set_color(0x0F, 0x00);
   vga_clear();

   klog_info("booting kernel");

   idt_init();
   idt_install_isrs();
   idt_install_irqs();

   pic_remap(0x20, 0x28);
   pic_mask_all();

   pic_unmask_irq(0); /* timer */
   pic_unmask_irq(1); /* keyboard */

   irq_init(100, 25);

   pmm_init(64U * 1024U * 1024U);
   uint32_t frame = pmm_alloc_frame();

   klog_info("interrupts configured");
   vga_puts("PMM free frames=");
   vga_putdec(pmm_free_frame_count());
   vga_puts(" alloc=");
   vga_puthex(frame);
   vga_puts(" VMM=");
   vga_puts(vmm_is_enabled() ? "ON" : "OFF");
   vga_puts("\n");

   asm volatile ("sti");

   shell_init();

   for(;;) {
       asm volatile ("hlt");
   }
}
