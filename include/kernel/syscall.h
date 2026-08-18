#ifndef SYSCALL_H
#define SYSCALL_H

#include <arch/x86/regs.h>

#define SYS_WRITE 1
#define SYS_EXIT  2

registers_t *syscall_handler(registers_t *regs);

uint32_t syscall_test_write(void);

#endif