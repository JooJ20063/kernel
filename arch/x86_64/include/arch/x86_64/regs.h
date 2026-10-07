#ifndef ARCH_X86_64_REGS_H
#define ARCH_X86_64_REGS_H

#include <stdint.h>

typedef struct regs {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rsi, rdi, rbp, rbx, rdx, rcx, rax;
    uint64_t int_no, err;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

#endif
