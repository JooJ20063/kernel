#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H

#include <stdint.h>
#include <arch/x86/regs.h>

typedef enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_ZOMBIE
} task_state_t;

typedef struct task_struct {
    uint32_t pid; // Process ID
    task_state_t state; // Current state of the task
    registers_t *context; // CPU context (registers)
;

    uint8_t *kernel_stack;
    uint32_t kernel_stack_size;

    uint32_t cr3;
    uint8_t *fpu_storage;
    uint8_t *fpu_area;
    uint32_t fpu_initialized;

    struct task_struct *next; // Pointer to the next task in the task list
} task_t;

void sched_init(uint32_t quantum_ticks);
registers_t *sched_tick_irq(registers_t *regs);

uint32_t sched_current_task(void);
uint32_t sched_current_pid(void);
uint32_t sched_switch_count(void);
uint32_t sched_task_count(void);

int sched_create_kernel_task(void (*entry)(void));
void sched_demo_init(void);

uint32_t sched_demo_counter_a(void);
uint32_t sched_demo_counter_b(void);

uint32_t sched_sse_value_a(void);
uint32_t sched_sse_value_b(void);

void sched_dump_tasks(void);
void task_list_tasks(void);

void task_yield(void);
void task_exit(void) __attribute__((noreturn));

task_t *sched_current_task_ptr(void);

#endif // KERNEL_TASK_H