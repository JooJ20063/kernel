#ifndef SYSCALL_H
#define SYSCALL_H

#include <arch/x86/regs.h>

#include <czk/syscall.h>

registers_t *syscall_handler(registers_t *regs);

uint32_t syscall_test_write(void);

#endif