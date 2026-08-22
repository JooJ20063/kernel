#include <arch/x86/idt.h>
#include <arch/x86/pic.h>
#include <arch/x86/irq.h>
#include <arch/x86/regs.h>
#include <kernel/vga.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/kmalloc.h>
#include <kernel/panic.h>
#include <kernel/klog.h>
#include <kernel/shell.h>
#include <kernel/ramfs.h>
#include <kernel/syscall.h>
#include <kernel/sched.h>
#include <arch/x86/fpu.h>
#include <arch/x86/tss.h>
#include <kernel/version.h>

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
extern uint8_t _user_text_start;
extern uint8_t _user_text_end;
extern uint8_t _user_data_start;
extern uint8_t _user_data_end;
extern uint8_t stack_top;

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

#define PF_PRESENT     0x1U
#define PF_WRITE       0x2U
#define PF_USER        0x4U
#define PF_RESERVED    0x8U
#define PF_INSTR       0x10U

#define KERNEL_STACK_LOW  0x00118000U
#define KERNEL_STACK_HIGH 0x0011C000U

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

static void map_user_sections(void) {
    if (map_range_flags(
        (uintptr_t)&_user_text_start,
        (uintptr_t)&_user_text_end,
        VMM_PAGE_USER) != 0) {
            kernel_panic("failed to map .usertext", 0);
        }
    
    if (map_range_flags(
        (uintptr_t)&_user_data_start,
        (uintptr_t)&_user_data_end,
        VMM_PAGE_USER | VMM_PAGE_RW) != 0) {
            kernel_panic("failed to map .userdata", 0);
        }
}

static void page_fault_classify(registers_t *r, uint32_t addr, uint32_t err) {
    vga_puts("Type: ");

    if ((err & PF_PRESENT) && (err & PF_WRITE)) {
        vga_puts("WRITE TO READ-ONLY PAGE");
    } else if (!(err & PF_PRESENT)) {
        uint32_t stack_guard = (r->esp & 0xFFFFF000U) - 0x1000U;

        if (addr == 0 || addr < 0x1000U) {
            vga_puts("NULL POINTER / INVALID ACCESS");
        } else if ((addr & 0xFFFFF000U) == stack_guard) {
            vga_puts("STACK OVERFLOW / GUARD PAGE");
        } else {
            vga_puts("NON-PRESENT PAGE / INVALID ACCESS");
        }
    } else if (err & PF_RESERVED) {
        vga_puts("RESERVED PAGE-TABLE BIT VIOLATION");
    } else if (err & PF_INSTR) {
        vga_puts("INSTRUCTION FETCH FAULT");
    } else {
        vga_puts("UNKNOWN PAGE FAULT");
    }

    vga_puts("\n");
}

static void print_pf_error(uint32_t err) {
    vga_puts("PF flags: ");

    vga_puts((err & 0x1) ? "PRESENT " : "NON-PRESEN T");
    vga_puts((err & 0x2) ? "WRITE " : "READ ");
    vga_puts((err & 0x4) ? "USER " : "KERNEL ");
    vga_puts((err & 0x8) ? "RESERVED-BIT " : "");
    vga_puts((err & 0x10) ? "INSTRUCTION-FETCH " : "");

    vga_puts("\n");

}

static uint32_t read_cr2(void) {
    uint32_t value;
    asm volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

static void page_fault_handler(registers_t *r) {
    uint32_t fault_addr = read_cr2();

    vga_set_color(0x0F, 0x04);
    vga_clear();

    vga_puts("*** PAGE FAULT ***\n\n");

    vga_puts("Fault address CR2: ");
    vga_puthex(fault_addr);
    vga_puts("\n");

    vga_puts("Error code: ");
    vga_puthex(r->err);
    vga_puts("\n");

    page_fault_classify(r, fault_addr, r->err);
    print_pf_error(r->err);

    vga_puts("\nCPU state:\n");

    vga_puts("EIP: ");
    vga_puthex(r->eip);
    vga_puts(" ESP: ");
    vga_puthex(r->esp);
    vga_puts(" EBP= ");
    vga_puthex(r->ebp);
    vga_puts("\n");

    vga_puts("EAX=");
    vga_puthex(r->eax);
    vga_puts(" EBX=");
    vga_puthex(r->ebx);
    vga_puts(" ECX=");
    vga_puthex(r->ecx);
    vga_puts(" EDX=");
    vga_puthex(r->edx);
    vga_puts("\n");

    vga_puts("CS=");
    vga_puthex(r->cs);
    vga_puts(" EFLAGS=");
    vga_puthex(r->eflags);
    vga_puts("\n");

    for (;;) {
        asm volatile("cli; hlt");
    }
}


registers_t *isr_handler_c(registers_t *r) {
    if (r->int_no == 7) {
        fpu_handle_nm();
        return r;
    }

    if (r->int_no == 128) {
        return syscall_handler(r);
    }

    if (r->int_no == 14) {
        page_fault_handler(r);
        return r;
    }

    const char *reason = "Unhandled exception";

    if (r->int_no < 32) {
        reason = exc[r->int_no].detail;
    }

    kernel_panic(reason, r);

    return r;
}

void kernel_main(uint32_t mb_info_addr) {
   vga_set_color(0x0F, 0x00);
   vga_clear();

   klog_info(CZK_NAME " (" CZK_SHORT_NAME "_x86)");

   tss_init();
   tss_set_kernel_stack((uint32_t)(uintptr_t)&stack_top);
   idt_init();
   idt_install_isrs();
   idt_install_irqs();

   pic_remap(0x20, 0x28);
   pic_mask_all();

   pic_unmask_irq(0); /* timer */
   pic_unmask_irq(1); /* keyboard */

   irq_init(100, 25);

    fpu_init();

   pmm_init_from_multiboot(mb_info_addr, (uintptr_t)&_kernel_start, (uintptr_t)&_kernel_end);
   vmm_init();
   protect_kernel_ro_sections();
   map_user_sections();
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
