#include <kernel/syscall.h>
#include <kernel/vga.h>
#include <kernel/task.h>
#include <kernel/sched.h>
#include <kernel/uaccess.h>
#include <kernel/fd.h>
#include <kernel/vfs.h>
#include <kernel/kmalloc.h>
#include <kernel/elf32.h>
#include <czk/errno.h>
#include <czk/fcntl.h>
#include <czk/seek.h>
#include <czk/stat.h>
#include <czk/dirent.h>

static uint32_t syscall_error(uint32_t error_number) {
    return (uint32_t)(-(int32_t)error_number);
}

typedef struct execve_copy {
    const char *argv[ELF32_EXEC_MAX_ARGS];
    const char *envp[ELF32_EXEC_MAX_ENVS];
    char argv_storage[ELF32_EXEC_MAX_ARGS][ELF32_EXEC_MAX_STRING];
    char envp_storage[ELF32_EXEC_MAX_ENVS][ELF32_EXEC_MAX_STRING];
    uint32_t argc;
    uint32_t envc;
} execve_copy_t;

static int syscall_copy_string_vector(
    uintptr_t user_vector,
    char storage[][ELF32_EXEC_MAX_STRING],
    const char **kernel_vector,
    uint32_t max_count,
    uint32_t *count_out
) {
    if (kernel_vector == 0 || count_out == 0) {
        return -1;
    }

    *count_out = 0U;

    if (user_vector == 0U) {
        return 0;
    }

    for (uint32_t i = 0U; i < max_count; ++i) {
        uint32_t user_string = 0U;
        int copy_result;

        if (copy_from_user(
                &user_string,
                (const void *)(
                    user_vector +
                    i * (uint32_t)sizeof(uint32_t)
                ),
                sizeof(user_string)) != 0) {
            return -1;
        }

        if (user_string == 0U) {
            *count_out = i;
            return 0;
        }

        copy_result = copy_string_from_user(
            storage[i],
            (const char *)(uintptr_t)user_string,
            ELF32_EXEC_MAX_STRING
        );

        if (copy_result == -1) {
            return -1;
        }

        if (copy_result == -2) {
            return -2;
        }

        kernel_vector[i] = storage[i];
    }

    return -3;
}

static uint32_t syscall_execve_error(int elf_status) {
    switch (elf_status) {
        case ELF32_ERR_NO_MEMORY:
            return CZK_ENOMEM;

        case ELF32_ERR_STACK_ARGS:
            return CZK_E2BIG;

        case ELF32_ERR_ARGUMENT:
            return CZK_EINVAL;

        default:
            return CZK_ENOEXEC;
    }
}

static void syscall_dirent_from_node(
    czk_dirent_t *dirent,
    const fs_node_t *node
) {
    uint32_t i = 0U;

    while (i < CZK_DIRENT_NAME_MAX) {
        dirent->d_name[i] = 0;
        i++;
    }

    i = 0U;
    while (i + 1U < CZK_DIRENT_NAME_MAX && node->name[i] != 0) {
        dirent->d_name[i] = node->name[i];
        i++;
    }

    dirent->d_name[i] = 0;
    dirent->d_flags = node->flags;
    dirent->d_size = node->size;
}

static int syscall_open_access(uint32_t flags, uint32_t *access_out) {
    uint32_t mode;

    if (access_out == 0) {
        return -1;
    }

    if ((flags & ~(CZK_O_ACCMODE | CZK_O_CREAT)) != 0U) {
        return -1;
    }

    mode = flags & CZK_O_ACCMODE;

    switch (mode) {
        case CZK_O_RDONLY:
            *access_out = FD_ACCESS_READ;
            return 0;

        case CZK_O_WRONLY:
            *access_out = FD_ACCESS_WRITE;
            return 0;

        case CZK_O_RDWR:
            *access_out = FD_ACCESS_READ | FD_ACCESS_WRITE;
            return 0;

        default:
            return -1;
    }
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
            process_t *process = sched_current_process_ptr();

            if (process == 0 || !fd_is_writable(&process->fds, fd)) {
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

                written = fd_write(&process->fds, fd, kernel_buf, chunk);
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

        case SYS_OPEN: {
            const char *user_path =
                (const char *)(uintptr_t)regs->ebx;
            uint32_t flags = regs->ecx;
            uint32_t access;
            char path[128];
            int copy_result;
            fs_node_t *node;
            int32_t fd;
            process_t *process = sched_current_process_ptr();

            if (process == 0) {
                regs->eax = syscall_error(CZK_EBADF);
                break;
            }

            if (syscall_open_access(flags, &access) != 0) {
                regs->eax = syscall_error(CZK_EINVAL);
                break;
            }

            copy_result = copy_string_from_user(
                path,
                user_path,
                sizeof(path)
            );

            if (copy_result == -1) {
                regs->eax = syscall_error(CZK_EFAULT);
                break;
            }

            if (copy_result == -2) {
                regs->eax = syscall_error(CZK_ENAMETOOLONG);
                break;
            }

            if (path[0] == 0) {
                regs->eax = syscall_error(CZK_EINVAL);
                break;
            }

            node = vfs_resolve(path);

            if (node == 0 && (flags & CZK_O_CREAT) != 0U) {
                node = vfs_create(
                    path,
                    FS_FILE | FS_WRITABLE
                );
                if (node == 0) {
                    regs->eax = syscall_error(CZK_ENOENT);
                    break;
                }
            }

            if (node == 0) {
                regs->eax = syscall_error(CZK_ENOENT);
                break;
            }

            fd = fd_open_vfs(&process->fds, node, access);
            if (fd == -1) {
                regs->eax = syscall_error(CZK_EMFILE);
                break;
            }

            if (fd == -2) {
                regs->eax = syscall_error(CZK_EACCES);
                break;
            }

            regs->eax = (uint32_t)fd;
            break;
        }

        case SYS_READ: {
            uint32_t fd = regs->ebx;
            uint8_t *user_buf =
                (uint8_t *)(uintptr_t)regs->ecx;
            uint32_t len = regs->edx;
            uint32_t total = 0U;
            uint8_t kernel_buf[128];
            process_t *process = sched_current_process_ptr();

            if (process == 0 || !fd_is_readable(&process->fds, fd)) {
                regs->eax = syscall_error(CZK_EBADF);
                break;
            }

            if (!user_ptr_valid(user_buf, len, 1)) {
                regs->eax = syscall_error(CZK_EFAULT);
                break;
            }

            while (total < len) {
                uint32_t remaining = len - total;
                uint32_t chunk = remaining;
                int32_t count;

                if (chunk > (uint32_t)sizeof(kernel_buf)) {
                    chunk = (uint32_t)sizeof(kernel_buf);
                }

                count = fd_read(&process->fds, fd, kernel_buf, chunk);
                if (count < 0) {
                    regs->eax = syscall_error(CZK_EBADF);
                    break;
                }

                if (count == 0) {
                    regs->eax = total;
                    break;
                }

                if (copy_to_user(
                        (void *)((uintptr_t)user_buf + total),
                        kernel_buf,
                        (uint32_t)count) != 0) {
                    regs->eax = syscall_error(CZK_EFAULT);
                    break;
                }

                total += (uint32_t)count;

                if ((uint32_t)count < chunk) {
                    regs->eax = total;
                    break;
                }
            }

            if (total == len) {
                regs->eax = total;
            }

            break;
        }

        case SYS_CLOSE: {
            uint32_t fd = regs->ebx;
            process_t *process = sched_current_process_ptr();

            if (process == 0 || fd_close(&process->fds, fd) != 0) {
                regs->eax = syscall_error(CZK_EBADF);
                break;
            }

            regs->eax = 0U;
            break;
        }

        case SYS_LSEEK: {
            uint32_t fd = regs->ebx;
            int32_t offset = (int32_t)regs->ecx;
            uint32_t whence = regs->edx;
            uint32_t new_offset = 0U;
            int32_t result;
            process_t *process = sched_current_process_ptr();

            if (process == 0) {
                regs->eax = syscall_error(CZK_EBADF);
                break;
            }

            result = fd_seek(
                &process->fds,
                fd,
                offset,
                whence,
                &new_offset
            );

            if (result == -1) {
                regs->eax = syscall_error(CZK_EBADF);
                break;
            }

            if (result == -2) {
                regs->eax = syscall_error(CZK_ESPIPE);
                break;
            }

            if (result == -3) {
                regs->eax = syscall_error(CZK_EINVAL);
                break;
            }

            regs->eax = new_offset;
            break;
        }

        case SYS_FSTAT: {
            uint32_t fd = regs->ebx;
            czk_stat_t *user_stat =
                (czk_stat_t *)(uintptr_t)regs->ecx;
            czk_stat_t stat;
            process_t *process = sched_current_process_ptr();

            if (process == 0 ||
                fd_stat(
                    &process->fds,
                    fd,
                    &stat.st_size,
                    &stat.st_flags) != 0) {
                regs->eax = syscall_error(CZK_EBADF);
                break;
            }

            if (copy_to_user(user_stat, &stat, sizeof(stat)) != 0) {
                regs->eax = syscall_error(CZK_EFAULT);
                break;
            }

            regs->eax = 0U;
            break;
        }

        case SYS_EXECVE: {
            const char *user_path =
                (const char *)(uintptr_t)regs->ebx;
            uintptr_t user_argv = (uintptr_t)regs->ecx;
            uintptr_t user_envp = (uintptr_t)regs->edx;
            char path[128];
            fs_node_t *node;
            execve_copy_t *copy;
            int copy_result;
            int status;

            copy_result = copy_string_from_user(
                path,
                user_path,
                sizeof(path)
            );

            if (copy_result == -1) {
                regs->eax = syscall_error(CZK_EFAULT);
                break;
            }

            if (copy_result == -2) {
                regs->eax = syscall_error(CZK_ENAMETOOLONG);
                break;
            }

            if (path[0] == 0) {
                regs->eax = syscall_error(CZK_EINVAL);
                break;
            }

            node = vfs_resolve(path);

            if (node == 0) {
                regs->eax = syscall_error(CZK_ENOENT);
                break;
            }

            if ((node->flags & FS_FILE) == 0U) {
                regs->eax = syscall_error(CZK_EACCES);
                break;
            }

            copy = (execve_copy_t *)kmalloc(sizeof(execve_copy_t));
            if (copy == 0) {
                regs->eax = syscall_error(CZK_ENOMEM);
                break;
            }

            copy_result = syscall_copy_string_vector(
                user_argv,
                copy->argv_storage,
                copy->argv,
                ELF32_EXEC_MAX_ARGS,
                &copy->argc
            );

            if (copy_result == 0) {
                copy_result = syscall_copy_string_vector(
                    user_envp,
                    copy->envp_storage,
                    copy->envp,
                    ELF32_EXEC_MAX_ENVS,
                    &copy->envc
                );
            }

            if (copy_result != 0) {
                kfree(copy);

                if (copy_result == -1) {
                    regs->eax = syscall_error(CZK_EFAULT);
                } else if (copy_result == -2) {
                    regs->eax = syscall_error(CZK_ENAMETOOLONG);
                } else {
                    regs->eax = syscall_error(CZK_E2BIG);
                }

                break;
            }

            status = elf32_exec_current(
                node,
                copy->argv,
                copy->argc,
                copy->envp,
                copy->envc,
                regs
            );

            kfree(copy);

            if (status != ELF32_OK) {
                regs->eax =
                    syscall_error(syscall_execve_error(status));
                break;
            }

            /*
             * Success never returns to the old image: regs now describes
             * the new ELF entry point and the scheduler has switched CR3.
             */
            return regs;
        }

        case SYS_READDIR: {
            uint32_t fd = regs->ebx;
            czk_dirent_t *user_dirent =
                (czk_dirent_t *)(uintptr_t)regs->ecx;
            czk_dirent_t dirent;
            fs_node_t *node = 0;
            int32_t result;
            process_t *process = sched_current_process_ptr();

            if (process == 0) {
                regs->eax = syscall_error(CZK_EBADF);
                break;
            }

            result = fd_readdir(&process->fds, fd, &node);

            if (result == -1) {
                regs->eax = syscall_error(CZK_EBADF);
                break;
            }

            if (result == -2) {
                regs->eax = syscall_error(CZK_ENOTDIR);
                break;
            }

            if (result == 0) {
                regs->eax = 0U;
                break;
            }

            syscall_dirent_from_node(&dirent, node);

            if (copy_to_user(
                    user_dirent,
                    &dirent,
                    sizeof(dirent)) != 0) {
                regs->eax = syscall_error(CZK_EFAULT);
                break;
            }

            regs->eax = 1U;
            break;
        }

        default:
            regs->eax = syscall_error(CZK_ENOSYS);
            break;
    }

    return regs;
}

uint32_t syscall_test_write(void) {
    extern uint8_t syscall_test_message;
    extern uint8_t syscall_test_message_end;
    uint32_t result;
    uint32_t length =
        (uint32_t)(
            (uintptr_t)&syscall_test_message_end -
            (uintptr_t)&syscall_test_message
        );

    asm volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_WRITE),
          "b"(1U),
          "c"(&syscall_test_message),
          "d"(length)
        : "memory"
    );

    return result;
}