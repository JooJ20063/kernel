#include <arch/x86/idt.h>
#include <arch/x86/regs.h>
#include <arch/x86/pic.h>
#include <kernel/vga.h>

static const char* exc[] = {
 "DIV0","DBG","NMI","BP","OF","BR","UD","NM","DF","CSO",
 "TS","NP","SS","GP","PF","RES","MF","AC","MC","XM",
 "VE","CP","22","23","24","25","26","27","28","29","30","31"
};

void isr_handler_c(struct regs* r){
    vga_set_cursor_pos(80);

    vga_puts("EXCEPTION: ");

    if (r->int_no < 32)
        vga_puts(exc[r->int_no]);
    else
        vga_puts("UNKNOWN");

    vga_puts("\nINT: ");
    vga_puthex(r->int_no);

    vga_puts(" ERR: ");
    vga_puthex(r->err);

    asm volatile ("cli");
    for (;;);
}

void kernel_main(void) {
   idt_init();
   idt_install_isrs();
   idt_install_irqs();

   pic_remap(0x20, 0x28);
   pic_mask_all();

   pic_unmask_irq(0);

   vga_clear();
   vga_write_at(0, "OK KERNEL !");

   asm volatile ("sti");

   for(;;);
}
