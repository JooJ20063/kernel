#include <kernel/syscall.h>
#include <kernel/vga.h>

registers_t *syscall_handler(registers_t *regs) {
    if (regs == 0) {
        return regs;
    }

    switch (regs->eax) {
        case SYS_WRITE: {
            uint32_t fd = regs->ebx;
            const char *buf = (const char *)(uintptr_t)regs->ecx;
            uint32_t len = regs->edx;

            if (fd != 1 || buf == 0) {
                regs->eax = 0xFFFFFFFFU;
                break;
            }

            for (uint32_t i = 0; i < len; i++) {
                vga_putc(buf[i]);
            }

            regs->eax = len;
            break;
        }

        case SYS_EXIT:
            /*
             * Implementaremos quando houver uma user task real.
             */
            regs->eax = 0xFFFFFFFFU;
            break;

        default:
            regs->eax = 0xFFFFFFFFU;
            break;
    }

    return regs;
}

uint32_t syscall_test_write(void) {
    static const char message[] = "hello from int 0x80\n";
    uint32_t result;

    asm volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_WRITE),
          "b"(1U),
          "c"(message),
          "d"((uint32_t)(sizeof(message) - 1U))
        : "memory"
    );

    return result;
}