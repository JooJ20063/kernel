#pragma once

#include <stdint.h>
#include <arch/x86/regs.h>

/* Configura a frequência do PIT (Timer) e inicializa o escalonador preemptivo */
void irq_init(uint32_t timer_hz, uint32_t scheduler_quantum_ticks);

/* Funções de consulta de telemetria do Timer (ticks acumulados, tempo decorrido e Hz) */
uint32_t irq_timer_ticks(void);
uint32_t irq_timer_seconds(void);
uint32_t irq_timer_hz(void);

/**
 * Dispatcher C para interrupções e exceções (chamado pelos stubs em Assembly).
 * Recebe o ponteiro do estado dos registradores da tarefa interrompida e pode
 * retornar um ponteiro diferente caso ocorra uma troca de contexto (preempção).
 */
registers_t *irq_handler_c(registers_t *regs);