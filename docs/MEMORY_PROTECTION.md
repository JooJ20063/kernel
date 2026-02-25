# Proteção de Memória (Plano)

Este documento descreve o plano de endurecimento de memória do kernel.

## Estado atual

- O PMM já trabalha com frames de 4 KiB.
- Ainda não há paginação (VMM) habilitada.
- O kernel roda integralmente em Ring 0.

## Objetivos de proteção

### 1) Paginação (VMM)

Implementar paginação para permitir:

- **Mapeamento de identidade inicial** dos primeiros blocos necessários (ex.: 1 MiB + regiões do kernel).
- **Proteção de código**: marcar páginas de `.text` como somente leitura (W=0).
- **Proteção de ponteiro nulo**: desmapear página zero para converter `NULL` em `#PF` controlado.

### 2) Isolamento de privilégios

- **Ring 0 (Kernel)**: acesso total a I/O e tabelas de memória.
- Futuro: Ring 3 para userland com transições controladas (syscalls/interrupt gates).

### 3) Stack e contexto

- ISR/IRQ preservam contexto em `registers_t`.
- Futuro: stacks dedicadas por tarefa e guard pages para detecção de overflow.

### 4) Validação de entrada

- Shell já limita buffer de linha (`SHELL_BUF`) e ignora overflow.
- Futuro: validação de comandos por gramática mínima e logs de auditoria.

## Hardware abstraction e segurança

Acesso a PIC/PIT/VGA/PS2 permanece no kernel. Em design futuro com userland:

- nenhum código de usuário poderá executar `inb/outb` diretamente;
- acesso a dispositivos será mediado por drivers/serviços do kernel.
