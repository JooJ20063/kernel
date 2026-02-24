#pragma once

#include <stdint.h>
#include <arch/x86/regs.h>

void irq_init(uint32_t timer_hz, uint32_t scheduler_quantum_ticks);
void irq_handler_c(registers_t *regs);
