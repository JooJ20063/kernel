#include <kernel/syscall.h>
#include <kernel/vga.h>
#include <kernel/task.h>
#include <kernel/sched.h>

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
            task_exit_code((int32_t)regs->ebx);
            __builtin_unreachable();
        
        case SYS_GETPID:
        regs->eax = sched_current_pid();
        break;

        case SYS_YIELD:
            return sched_yield_irq(regs);

        
        case SYS_GETPPID:
            regs->eax = sched_current_ppid();
            break;

        case SYS_SLEEP:
            if (regs->ebx != 0) {
                task_sleep_prepare((uint32_t)regs->ebx);
            }

            regs->eax = 0;
            return sched_yield_irq(regs);
        
        case SYS_WAIT:
            int32_t status = 0;
            int32_t pid = task_wait_child(&status);
            
            regs->eax = (uint32_t)regs->eax;
            regs->edx = (uint32_t)regs->edx;
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