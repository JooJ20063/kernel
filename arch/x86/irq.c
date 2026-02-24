#include <arch/x86/irq.h>
#include <arch/x86/pic.h>

void irq_handler_c(registers_t *regs) {
    if (regs->int_no >= 32 && regs->int_no <= 47) {
        pic_send_eoi(regs->int_no - 32);
    }
}
