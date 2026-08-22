#ifndef SYSCALL_H
#define SYSCALL_H

#include <arch/x86/regs.h>

#define SYS_WRITE   1
#define SYS_EXIT    2
#define SYS_GETPID  3
#define SYS_YIELD   4
#define SYS_GETPPID 5
#define SYS_SLEEP   6
#define SYS_WAIT    7

registers_t *syscall_handler(registers_t *regs);

uint32_t syscall_test_write(void);

#endif