#ifndef SYSCALL_H
#define SYSCALL_H

#include <arch/x86/regs.h>

/* Cabeçalho de definições globais e números de syscalls da Userspace / C library (czk) */
#include <czk/syscall.h>

/**
 * Handler central de interrupção de Chamadas de Sistema (Syscalls).
 * Recebe o ponteiro para o contexto salvo de registradores (EAX=número da syscall, EBX/ECX/EDX=argumentos),
 * executa a rotina correspondente e retorna o ponteiro atualizado do contexto de registradores.
 */
registers_t *syscall_handler(registers_t *regs);

/* Função de teste interna do kernel para validação do mecanismo de escrita via syscall */
uint32_t syscall_test_write(void);

#endif