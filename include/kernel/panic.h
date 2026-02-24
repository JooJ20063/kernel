#pragma once

#include <arch/x86/regs.h>

void kernel_panic(const char *reason, const registers_t *regs);
