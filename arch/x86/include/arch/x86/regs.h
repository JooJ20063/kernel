#ifndef ARCH_X86_REGS_H
#define ARCH_X86_REGS_H

#include <stdint.h>

/**
 * Representa o frame da pilha salvo durante uma interrupção/exceção no modo 32-bit.
 * Reflete exatamente a ordem inversa em que os registradores são empilhados no stubs Assembly.
 */
typedef struct regs {
    /* Registradores de segmento salvos manualmente pelo stub */
    uint32_t gs, fs, es, ds;
    
    /* Registradores de uso geral salvos pela instrução 'pusha' */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    
    /* Número da interrupção (vetor IDT) e código de erro (empilhado pela CPU ou dummy 0 pelo stub) */
    uint32_t int_no, err;
    
    /* Frame de retorno empilhado automaticamente pelo hardware x86 na ocorrência da interrupção */
    uint32_t eip, cs, eflags;
    
    /* Empilhados automaticamente pela CPU APENAS se houver mudança de Ring (ex: Ring 3 -> Ring 0) */
    uint32_t useresp, ss;
} registers_t;

#endif