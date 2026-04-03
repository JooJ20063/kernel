#pragma once

#ifdef __x86_64__
#include <arch/x86_64/regs.h>
#else
#include <arch/x86/regs.h>
#endif

void kernel_panic(const char *reason, const registers_t *regs);
