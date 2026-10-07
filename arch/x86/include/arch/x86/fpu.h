#ifndef ARCH_X86_FPU_H
#define ARCH_X86_FPU_H

#include <stdint.h>
#include <kernel/task.h>

#define FPU_SAVE_AREA_SIZE 512
#define FPU_SAVE_AREA_ALIGN 16

void fpu_init(void);
void fpu_set_ts(void);
void fpu_clear_ts(void);

void fpu_handle_nm(void);

void fpu_init_task(task_t *task);
void fpu_free_task(task_t *task);

#endif