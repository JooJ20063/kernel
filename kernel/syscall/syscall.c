#include <kernel/syscall.h>
#include <kernel/vga.h>
#include <kernel/task.h>
#include <kernel/sched.h>
#include <kernel/uaccess.h>
#include <kernel/fd.h>
#include <czk/errno.h>

static uint32_t syscall_error(uint32_t error_number) {
    return (uint32_t)(-(int32_t)error_number);
}

registers_t *syscall_handler(registers_t *regs) {
    if (regs == 0) {
        return regs;
    }

    switch (regs->eax) {
        case SYS_WRITE: {
            uint32_t fd = regs->ebx;
            const uint8_t *buf =
                (const uint8_t *)(uintptr_t)regs->ecx;
            uint32_t len = regs->edx;
            uint32_t offset = 0U;
            uint8_t kernel_buf[128];
            task_t *task = sched_current_task_ptr();

            if (task == 0 || !fd_is_writable(&task->fds, fd)) {
                regs->eax = syscall_error(CZK_EBADF);
                break;
            }

            /*
             * Validate the whole userspace range before producing output.
             * This prevents a write from partially succeeding and then
             * discovering that a later page is not accessible to Ring 3.
             */
            if (!user_ptr_valid(buf, len, 0)) {
                regs->eax = syscall_error(CZK_EFAULT);
                break;
            }

            while (offset < len) {
                uint32_t remaining = len - offset;
                uint32_t chunk = remaining;
                int32_t written;

                if (chunk > (uint32_t)sizeof(kernel_buf)) {
                    chunk = (uint32_t)sizeof(kernel_buf);
                }

                if (copy_from_user(
                        kernel_buf,
                        (const void *)((uintptr_t)buf + offset),
                        chunk) != 0) {
                    regs->eax = syscall_error(CZK_EFAULT);
                    break;
                }

                written = fd_write(&task->fds, fd, kernel_buf, chunk);
                if (written < 0) {
                    regs->eax = syscall_error(CZK_EBADF);
                    break;
                }

                offset += (uint32_t)written;

                if ((uint32_t)written < chunk) {
                    regs->eax = offset;
                    break;
                }
            }

            if (offset == len) {
                regs->eax = len;
            }

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
        
        case SYS_WAIT: {
            int32_t *user_status =
                (int32_t *)(uintptr_t)regs->ebx;
            int32_t status = 0;
            int32_t pid;

            /*
             * Validate the userspace destination before reaping the child.
             * A bad status pointer must not consume a zombie process.
             * A null pointer is valid and behaves like wait(NULL).
             */
            if (user_status != 0 &&
                !user_ptr_valid(user_status, sizeof(status), 1)) {
                regs->eax = syscall_error(CZK_EFAULT);
                break;
            }

            pid = task_wait_child(&status);
            if (pid < 0) {
                regs->eax = syscall_error(CZK_ECHILD);
                break;
            }

            if (user_status != 0 &&
                copy_to_user(user_status, &status, sizeof(status)) != 0) {
                regs->eax = syscall_error(CZK_EFAULT);
                break;
            }

            regs->eax = (uint32_t)pid;
            break;
        }

        default:
            regs->eax = syscall_error(CZK_ENOSYS);
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