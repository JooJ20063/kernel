#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H

#include <stdint.h>
#include <arch/x86/regs.h>
#include <kernel/process.h>

typedef enum task_state {
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_ZOMBIE
} task_state_t;

typedef enum task_block_reason {
    TASK_BLOCK_NONE = 0,
    TASK_BLOCK_SLEEP,
    TASK_BLOCK_EVENT
} task_block_reason_t;

typedef struct task_struct {
    uint32_t tid;
    process_t *process;

    task_state_t state;
    task_block_reason_t block_reason;
    registers_t *context;
    uint32_t wake_tick;

    uint8_t *kernel_stack;
    uint32_t kernel_stack_size;

    uint8_t *fpu_storage;
    uint8_t *fpu_area;
    uint32_t fpu_initialized;

    struct task_struct *next;
    struct task_struct *wait_next;
} task_t;

typedef struct wait_queue {
    task_t *head;
    task_t *tail;
} wait_queue_t;

typedef int (*wait_condition_t)(void *ctx);

void sched_init(uint32_t quantum_ticks);
void sched_set_bootstrap_address_space(uint32_t cr3);
registers_t *sched_tick_irq(registers_t *regs);
registers_t *sched_yield_irq(registers_t *regs);

uint32_t sched_current_task(void);
uint32_t sched_current_pid(void);
uint32_t sched_switch_count(void);
uint32_t sched_task_count(void);
uint32_t sched_last_exit_pid(void);
uint32_t sched_current_ppid(void);
int32_t sched_last_exit_code(void);
int32_t task_wait_child(int32_t *status);
int32_t task_wait_child_blocking(int32_t *status);

int sched_create_kernel_task(const char *name, void (*entry)(void));
int sched_create_user_task(const char *name, void (*entry)(void), uintptr_t user_stack_top);

/*
 * Creates a Ring 3 task in an already prepared private address space.
 * Ownership of cr3 transfers to the new process only on success.
 */
int sched_create_user_task_in_address_space(
    const char *name,
    uintptr_t entry,
    uintptr_t user_stack_top,
    uint32_t cr3
);

int sched_exec_current_address_space(
    const char *name,
    uintptr_t entry,
    uintptr_t user_stack_top,
    uint32_t cr3,
    registers_t *regs
);

void sched_demo_init(void);

uint32_t sched_demo_counter_a(void);
uint32_t sched_demo_counter_b(void);
uint32_t sched_yield_counter(void);
uint32_t sched_exit_task_run(void);
uint32_t sched_sleep_demo_counter(void);
uint32_t sched_sse_value_a(void);
uint32_t sched_sse_value_b(void);
uint32_t sched_event_demo_counter(void);
uint32_t sched_event_waker_counter(void);
uint32_t sched_event_a_counter(void);
uint32_t sched_event_b_counter(void);

void sched_dump_tasks(void);
void task_list_tasks(void);
void task_sleep_ticks(uint32_t ticks);
void task_sleep_prepare(uint32_t ticks);
void wait_queue_init(wait_queue_t *queue);
int wait_queue_wake_one(wait_queue_t *queue);
void wait_queue_wake_all(wait_queue_t *queue);
void task_wait(wait_queue_t *queue);
int task_wait_until(wait_queue_t *queue, wait_condition_t condition, void *ctx);
void task_yield(void);
void task_exit_code(int32_t exit_code) __attribute__((noreturn));
void task_exit(void) __attribute__((noreturn));
registers_t *task_exit_from_exception(registers_t *regs, int32_t exit_code);
void task_block(void);
void task_wake(task_t *task);

task_t *sched_current_task_ptr(void);
process_t *sched_current_process_ptr(void);

#endif // KERNEL_TASK_H