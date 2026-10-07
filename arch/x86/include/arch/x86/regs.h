#ifndef ARCH_X86_REGS_H
#define ARCH_X86_REGS_H

#include <stdint.h>

typedef struct regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err;
    uint32_t eip, cs, eflags, useresp, ss;
} registers_t ;

#endif
