#include <arch/x86/idt.h>
#include <arch/x86/regs.h>
#include <arch/x86/pic.h>

static const char* exc[] = {
 "DIV0","DBG","NMI","BP","OF","BR","UD","NM","DF","CSO",
 "TS","NP","SS","GP","PF","RES","MF","AC","MC","XM",
 "VE","CP","22","23","24","25","26","27","28","29","30","31"
};

static volatile char* vga = (volatile char*)0xB8000;
static int cursor = 0;

static void vga_putc(char c){
    vga[cursor++] = c;
    vga[cursor++] = 0x0F;
}

static void vga_puts(const char* s){
    while (*s)
        vga_putc(*s++);
}

static void vga_puthex(uint32_t v){
    const char* h = "0123456789ABCDEF";
    vga_puts("0x");
    for (int i = 28; i >= 0; i -=4)
        vga_putc(h[(v >> i) & 0xF]);
}


void isr_handler_c(struct regs* r){
    cursor = 160;

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

    volatile char* vga = (volatile char*)0xB8000;
    vga[0]  = 'O';
    vga[1]  = 0x0F;
    vga[2]  = 'K';
    vga[3]  = 0x0F;
    vga[4]  = ' ';
    vga[5]  = 0x0F;
    vga[6]  = 'K';
    vga[7]  = 0x0F;
    vga[8]  = 'E';
    vga[9]  = 0x0F;
    vga[10] = 'R';
    vga[11] = 0x0F;
    vga[12] = 'N';
    vga[13] = 0x0F;
    vga[14] = 'E';
    vga[15] = 0x0F;
    vga[16] = 'L';
    vga[17] = 0x0F;
    vga[18] = ' ';
    vga[19] = 0x0F;
    vga[20] = '!';
    vga[21] = 0x0F;


    asm volatile ("sti");
   // asm volatile ("int $0x03");

    for(;;);
}
