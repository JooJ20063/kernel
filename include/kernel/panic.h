#pragma once

/* Seleção condicional da estrutura de registradores de acordo com a arquitetura-alvo */
#ifdef __x86_64__
#include <arch/x86_64/regs.h>
#else
#include <arch/x86/regs.h>
#endif

/**
 * Interrompe a execução do sistema e exibe uma mensagem de falha fatal (Kernel Panic).
 * Imprime o motivo do erro e realiza o dump completo dos registradores passados em 'regs'
 * antes de desativar as interrupções e travar a CPU.
 */
void kernel_panic(const char *reason, const registers_t *regs);