#include <arch/x86/idt.h>
#include <arch/x86/pic.h>
#include <arch/x86/irq.h>
#include <kernel/vga.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/kmalloc.h>
#include <kernel/panic.h>
#include <kernel/klog.h>
#include <kernel/shell.h>
#include <kernel/ramfs.h>
#include <kernel/syscall.h>

struct exception_info {
    const char *name;
    const char *detail;
};

extern uint8_t _kernel_start;
extern uint8_t _kernel_end;
extern uint8_t _text_start;
extern uint8_t _text_end;
extern uint8_t _rodata_start;
extern uint8_t _rodata_end;

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

static int map_range_flags(uintptr_t start, uintptr_t end, uint32_t flags) {
    uintptr_t page_start = start & ~(uintptr_t)0xFFFU;
    uintptr_t page_end = (end + 0xFFFU) & ~(uintptr_t)0xFFFU;

    for (uintptr_t addr = page_start; addr < page_end; addr += 0x1000U) {
        if (vmm_map_page(addr, addr, flags) != 0) {
            return -1;
        }
    }

    return 0;
}

static void protect_kernel_ro_sections(void) {
    if (map_range_flags((uintptr_t)&_text_start, (uintptr_t)&_text_end, 0) != 0) {
        kernel_panic("failed to protect .text", 0);
    }

    if (map_range_flags((uintptr_t)&_rodata_start, (uintptr_t)&_rodata_end, 0) != 0) {
        kernel_panic("failed to protect .rodata", 0);
    }
}

static void syscall_handler(registers_t *r) {
    uint32_t syscall_num = r->eax;

    switch (syscall_num) {
        case SYS_WRITE: { // write
            int fd = r->ebx;
            const char *buf = (const char *)r->ecx;
            uint32_t len = r->edx;
            if (fd == 1) { // stdout
                for (uint32_t i = 0; i < len; i++) {
                    vga_putc(buf[i]);
                }
            }
            r->eax = len; // return written bytes
            break;
        }
        case SYS_EXIT: { // exit
            vga_puts("Exiting via syscall...\n");
            asm volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
            asm volatile ("cli");
            for (;;) {
                asm volatile ("hlt");
            }
            break;
        }
        default:
            r->eax = -1; // error
            break;
    }
}

void isr_handler_c(registers_t *r){
    if (r->int_no == 128) {
        syscall_handler(r);
        return;
    }

    const char *reason = "Unhandled exception";

    if (r->int_no < 32) {
        reason = exc[r->int_no].detail;
    }

    kernel_panic(reason, r);
}

void kernel_main(uint32_t mb_info_addr) {
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

   pmm_init_from_multiboot(mb_info_addr, (uintptr_t)&_kernel_start, (uintptr_t)&_kernel_end);
   vmm_init();
   protect_kernel_ro_sections();
   kmalloc_init();
   init_ramfs(0, 0);

   klog_info("interrupts configured");
   vga_puts("PMM free frames=");
   vga_putdec(pmm_free_frame_count());
   vga_puts(" VMM=");
   vga_puts(vmm_is_enabled() ? "ON" : "OFF");
   vga_puts(" WP=");
   vga_puts(vmm_wp_is_enabled() ? "ON" : "OFF");
   vga_puts(" null-guard=ON ramfs=ON\n");

   asm volatile ("sti");

   shell_init();

   for(;;) {
       asm volatile ("hlt");
   }
}
