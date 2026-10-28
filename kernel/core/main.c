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
#include <kernel/ring0_shell.h>
#include <kernel/ramfs.h>
#include <kernel/devfs.h>
#include <kernel/syscall.h>
#include <kernel/sched.h>
#include <arch/x86/fpu.h>
#include <arch/x86/tss.h>
#include <kernel/version.h>
#include <kernel/serial.h>
#include <kernel/fd.h>
#include <kernel/vfs.h>
#include <kernel/multiboot2.h>

/* Descrição de exceções e falhas da arquitetura x86 (Vetores 0 a 31) */
struct exception_info {
    const char *name;
    const char *detail;
};

/* Símbolos exportados pelo Linker Script para delimitação das seções da memória do Kernel */
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

/* Tabela de nomes e siglas das exceções da CPU x86 */
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

/* Flags do código de erro de Page Fault (alinhados com a arquitetura x86) */
#define PF_PRESENT     0x1U  // 0: Página não presente, 1: Violação de proteção
#define PF_WRITE       0x2U  // 0: Acesso de Leitura, 1: Acesso de Escrita
#define PF_USER        0x4U  // 0: Execução em Ring 0, 1: Execução em Ring 3
#define PF_RESERVED    0x8U  // Bit reservado sobrescrito no Page Directory/Table
#define PF_INSTR       0x10U // Falha durante busca de instrução (Instruction Fetch)

#define KERNEL_STACK_LOW  0x00118000U
#define KERNEL_STACK_HIGH 0x0011C000U

/**
 * Varre a estrutura de tags do Multiboot2 enviada pelo bootloader para localizar
 * o primeiro módulo pré-carregado na memória física (utilizado para o initrd/ramfs).
 */
static int multiboot2_first_module(
    uint32_t mb_info_addr,
    uintptr_t *start_out,
    uintptr_t *end_out
) {
    uint32_t total_size;
    uintptr_t info_end;
    struct multiboot2_tag *tag;

    if (mb_info_addr == 0U || start_out == 0 || end_out == 0) {
        return -1;
    }

    total_size = *(uint32_t *)(uintptr_t)mb_info_addr;
    info_end = (uintptr_t)mb_info_addr + (uintptr_t)total_size;
    tag = (struct multiboot2_tag *)((uintptr_t)mb_info_addr + 8U);

    while ((uintptr_t)tag < info_end &&
           tag->type != MULTIBOOT2_TAG_END) {
        if (tag->type == MULTIBOOT2_TAG_MODULE) {
            struct multiboot2_tag_module *module =
                (struct multiboot2_tag_module *)tag;

            if (module->mod_end > module->mod_start) {
                *start_out = (uintptr_t)module->mod_start;
                *end_out = (uintptr_t)module->mod_end;
                return 0;
            }
        }

        /* Avança para a próxima tag respeitando o alinhamento de 8 bytes */
        tag = (struct multiboot2_tag *)(
            ((uintptr_t)tag + tag->size + 7U) &
            ~(uintptr_t)7U
        );
    }

    return -1;
}

/* Mapeia um intervalo de memória física de forma alinhada a páginas de 4KB no VMM */
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

/* Aplica proteção de leitura/execução nas seções do Kernel (.text e .rodata) desativando o bit R/W */
static void protect_kernel_ro_sections(void) {
    if (map_range_flags((uintptr_t)&_text_start, (uintptr_t)&_text_end, 0) != 0) {
        kernel_panic("failed to protect .text", 0);
    }

    if (map_range_flags((uintptr_t)&_rodata_start, (uintptr_t)&_rodata_end, 0) != 0) {
        kernel_panic("failed to protect .rodata", 0);
    }
}

/* Mapeia seções reservadas para executáveis de usuários com privilégios de Ring 3 (VMM_PAGE_USER) */
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

/* Diagnostica a causa provável de um Page Fault com base no endereço e no código de erro */
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

/* Imprime a decomposição das flags de erro lançadas pela exceção #PF da CPU */
static void print_pf_error(uint32_t err) {
    vga_puts("PF flags: ");

    vga_puts((err & 0x1) ? "PRESENT " : "NON-PRESENT ");
    vga_puts((err & 0x2) ? "WRITE " : "READ ");
    vga_puts((err & 0x4) ? "USER " : "KERNEL ");
    vga_puts((err & 0x8) ? "RESERVED-BIT " : "");
    vga_puts((err & 0x10) ? "INSTRUCTION-FETCH " : "");

    vga_puts("\n");
}

/* Lê o registrador de controle CR2 (contém o endereço virtual que causou o Page Fault) */
static uint32_t read_cr2(void) {
    uint32_t value;
    asm volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

/* Handler específico da exceção #PF (Page Fault - Vetor 14) */
static registers_t *page_fault_handler(registers_t *r) {
    uint32_t fault_addr = read_cr2();

    /* Se a falha ocorreu em Ring 3, encerra apenas a tarefa ofensora e evita o Kernel Panic */
    if ((r->cs & 0x3U) == 0x3U) {
        vga_puts("[user fault] pid=");
        vga_putdec(sched_current_pid());
        vga_puts(" page fault at ");
        vga_puthex(fault_addr);
        vga_puts(" err=");
        vga_puthex(r->err);
        vga_puts("; task terminated\n");

        return task_exit_from_exception(r, -14);
    }

    /* Page Fault em Ring 0 é considerado um erro fatal do sistema */
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

/* Identifica se uma exceção disparada em User Space pode ser tratada finalizando o processo */
static int user_exception_is_recoverable(uint32_t vector) {
    switch (vector) {
        case 0:  /* #DE - Divide Error */
        case 1:  /* #DB - Debug */
        case 3:  /* #BP - Breakpoint */
        case 4:  /* #OF - Overflow */
        case 5:  /* #BR - BOUND Range Exceeded */
        case 6:  /* #UD - Invalid Opcode */
        case 10: /* #TS - Invalid TSS */
        case 11: /* #NP - Segment Not Present */
        case 12: /* #SS - Stack Segment Fault */
        case 13: /* #GP - General Protection */
        case 14: /* #PF - Page Fault */
        case 16: /* #MF - x87 Floating-Point */
        case 17: /* #AC - Alignment Check */
        case 19: /* #XM - SIMD Floating-Point */
        case 21: /* #CP - Control Protection */
            return 1;

        default:
            return 0;
    }
}

/* Handler genérico para capturar falhas e exceções acionadas por processos em Ring 3 */
static registers_t *user_exception_handler(registers_t *r) {
    const char *name = "exception";

    if (r->int_no < 32U) {
        name = exc[r->int_no].name;
    }

    vga_puts("[user fault] pid=");
    vga_putdec(sched_current_pid());
    vga_puts(" ");
    vga_puts(name);
    vga_puts(" vector=");
    vga_putdec(r->int_no);
    vga_puts(" err=");
    vga_puthex(r->err);
    vga_puts("; task terminated\n");

    return task_exit_from_exception(r, -(int32_t)r->int_no);
}

/**
 * Dispatcher central de interrupções e exceções (chamado pelo assembly de baixa camada idt.s).
 */
registers_t *isr_handler_c(registers_t *r) {
    /* Exceção #NM (Device Not Available): usada para carregamento preguiçoso da FPU (Lazy FPU State Restore) */
    if (r->int_no == 7) {
        fpu_handle_nm();
        return r;
    }

    /* Interrupção de Chamada de Sistema (Syscall) */
    if (r->int_no == 128) {
        return syscall_handler(r);
    }

    /* Exceção de Page Fault */
    if (r->int_no == 14) {
        return page_fault_handler(r);
    }

    /* Outras exceções tratáveis originadas em User Space (Ring 3) */
    if (r->int_no < 32U &&
        (r->cs & 0x3U) == 0x3U &&
        user_exception_is_recoverable(r->int_no)) {
        return user_exception_handler(r);
    }

    /* Exceção não tratada em Ring 0 aciona parada de emergência (Kernel Panic) */
    const char *reason = "Unhandled exception";

    if (r->int_no < 32U) {
        reason = exc[r->int_no].detail;
    }

    kernel_panic(reason, r);

    return r;
}

/**
 * Ponto de entrada principal do Kernel em C (chamado após a montagem do ambiente de boot em assembly).
 */
void kernel_main(uint32_t mb_info_addr) {
   uintptr_t initrd_phys_start = 0U;
   uintptr_t initrd_phys_end = 0U;

   serial_init();

   vga_set_color(0x0F, 0x00);
   vga_clear();

   klog_info(CZK_NAME " (" CZK_SHORT_NAME "_x86)");

   /* Configuração de Tabela de Descritores de Interrupção e Segmente de Tarefa */
   tss_init();
   tss_set_kernel_stack((uint32_t)(uintptr_t)&stack_top);
   idt_init();
   idt_install_isrs();
   idt_install_irqs();

   /* Remapeamento do controlador PIC 8259 (Offset 0x20 e 0x28) e desativação inicial das IRQs */
   pic_remap(0x20, 0x28);
   pic_mask_all();

   /* Habilitação de interrupções de hardware essenciais */
   pic_unmask_irq(0); /* Timer PIT */
   pic_unmask_irq(1); /* Teclado PS/2 */
   if (serial_is_available()) {
       serial_enable_rx_interrupt();
       pic_unmask_irq(4); /* Porta Serial COM1 */
   }

   irq_init(100, 25);
   fpu_init();

   /* Localiza módulo initrd nos dados repassados pelo bootloader */
   (void)multiboot2_first_module(
       mb_info_addr,
       &initrd_phys_start,
       &initrd_phys_end
   );

   /* Inicialização dos gerenciadores de memória física (PMM), virtual (VMM) e Heap (kmalloc) */
   pmm_init_from_multiboot(mb_info_addr, (uintptr_t)&_kernel_start, (uintptr_t)&_kernel_end);
   vmm_init();
   sched_set_bootstrap_address_space(vmm_kernel_cr3());
   protect_kernel_ro_sections();
   map_user_sections();
   kmalloc_init();

   /* Carregamento do RAMFS a partir do módulo da initrd na memória física */
   if (initrd_phys_end > initrd_phys_start) {
       uint32_t initrd_size =
           (uint32_t)(initrd_phys_end - initrd_phys_start);
       uint8_t *initrd_copy =
           (uint8_t *)kmalloc(initrd_size);

       if (initrd_copy == 0) {
           kernel_panic("failed to allocate initrd copy", 0);
       }

       if (vmm_copy_from_phys(
               initrd_copy,
               initrd_phys_start,
               initrd_size) != 0) {
           kernel_panic("failed to copy initrd module", 0);
       }

       init_ramfs(
           (uintptr_t)initrd_copy,
           (uintptr_t)initrd_copy + initrd_size
       );
   } else {
       init_ramfs(0, 0);
   }

   /* Montagem do sistema de arquivos de dispositivos virtuais (/dev) */
   if (devfs_init() != 0) {
       kernel_panic("failed to mount devfs", 0);
   }

   /* Associa entrada e saída padrão (stdin/stdout/stderr) do processo inicial ao /dev/tty1 */
   {
       process_t *bootstrap = sched_current_process_ptr();
       fs_node_t *tty = vfs_resolve("/dev/tty1");

       if (bootstrap == 0 ||
           tty == 0 ||
           fd_table_bind_stdio(&bootstrap->fds, tty) != 0) {
           kernel_panic("failed to bind bootstrap stdio", 0);
       }
   }

   klog_info("interrupts configured");
   vga_puts("PMM free frames=");
   vga_putdec(pmm_free_frame_count());
   vga_puts(" VMM=");
   vga_puts(vmm_is_enabled() ? "ON" : "OFF");
   vga_puts(" WP=");
   vga_puts(vmm_wp_is_enabled() ? "ON" : "OFF");
   vga_puts(" null-guard=ON ramfs=ON devfs=ON\n");

   /* Habilita interrupções de hardware na CPU x86 (Assembly 'sti') */
   asm volatile ("sti");

   /* Inicializa o Shell interativo em Ring 0 */
   ring0_shell_init();

   /* Loop ocioso do Kernel (Kernel Idle Loop) em instrução HLT */
   for(;;) {
       asm volatile ("hlt");
   }
}