#ifndef ARCH_X86_FPU_H
#define ARCH_X86_FPU_H

#include <stdint.h>
#include <kernel/task.h>

/* Tamanho exato em bytes do bloco exigido pelas instruções FXSAVE e FXRSTOR */
#define FPU_SAVE_AREA_SIZE 512

/* Requisito estrito de alinhamento em memória (16 bytes) para o buffer de contexto SSE/FPU */
#define FPU_SAVE_AREA_ALIGN 16

/* Inicializa registradores de controle (CR0/CR4) e aciona FNINIT */
void fpu_init(void);

/* Manipulação do bit CR0.TS (Task Switched) para controle de Lazy FPU Context Switching */
void fpu_set_ts(void);
void fpu_clear_ts(void);

/* Handler da Exceção #NM (Device Not Available / Int 7) */
void fpu_handle_nm(void);

/* Gerenciamento do ciclo de vida da área de salvamento do contexto da FPU por tarefa */
void fpu_init_task(task_t *task);
void fpu_reset_task(task_t *task);
void fpu_free_task(task_t *task);

#endif