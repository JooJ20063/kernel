#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H

#include <stdint.h>
#include <arch/x86/regs.h>
#include <kernel/process.h>

/* Estados possíveis de um fluxo de execução (Task) no ciclo de vida */
typedef enum task_state {
    TASK_UNUSED = 0, // Entrada inativa na tabela de tarefas
    TASK_READY,      // Pronta para ser executada e na fila de escalonamento
    TASK_RUNNING,    // Atualmente em execução pela CPU
    TASK_BLOCKED,    // Bloqueada aguardando evento, tempo (sleep) ou E/S
    TASK_ZOMBIE      // Execução encerrada, aguardando limpeza pelo processo pai
} task_state_t;

/* Motivos de bloqueio da tarefa */
typedef enum task_block_reason {
    TASK_BLOCK_NONE = 0, // Não bloqueada
    TASK_BLOCK_SLEEP,    // Bloqueada por temporização (timer ticks)
    TASK_BLOCK_EVENT     // Bloqueada aguardando sinalização em Wait Queue
} task_block_reason_t;

/**
 * Estrutura do Bloco de Controle de Tarefa (TCB - Task Control Block / Thread).
 * Armazena o contexto de registradores da CPU, pilha de kernel isolada,
 * estado da FPU/SSE e ligações com a PCB do processo pai.
 */
typedef struct task_struct {
    uint32_t tid;            // Identificador único da tarefa (Thread ID)
    process_t *process;      // Ponteiro para o processo proprietário (PCB com espaço virtual/CR3)

    task_state_t state;              // Estado atual de execução
    task_block_reason_t block_reason; // Causa do bloqueio (se state == TASK_BLOCKED)
    registers_t *context;            // Ponteiro para os registradores salvos na pilha do Kernel no último context switch
    uint32_t wake_tick;              // Tick do PIT em que a tarefa deve acordar (se em TASK_BLOCK_SLEEP)

    uint8_t *kernel_stack;           // Endereço base da pilha de Kernel de Ring 0 dedicada
    uint32_t kernel_stack_size;      // Tamanho em bytes da pilha de Kernel

    uint8_t *fpu_storage;            // Buffer alinhado em memória para armazenamento dos registradores FPU/MMX/SSE
    uint8_t *fpu_area;               // Ponteiro efetivo alinhado (512 bytes para FXSAVE/FXRSTOR)
    uint32_t fpu_initialized;        // Flag indicando se a FPU foi inicializada para esta tarefa

    struct task_struct *next;        // Próxima tarefa na lista encadeada geral ou do escalonador
    struct task_struct *wait_next;   // Próxima tarefa na lista encadeada de uma fila de espera (wait_queue_t)
} task_t;

/* Fila de espera (Wait Queue) para bloqueio/desbloqueio sincronizado de tarefas */
typedef struct wait_queue {
    task_t *head; // Primeira tarefa aguardando na fila
    task_t *tail; // Última tarefa aguardando na fila
} wait_queue_t;

/* Tipo de função de ponteiro para verificação de condição de desbloqueio condicional */
typedef int (*wait_condition_t)(void *ctx);

/* Inicialização e controle do Escalonador */
void sched_init(uint32_t quantum_ticks);
void sched_set_bootstrap_address_space(uint32_t cr3);
registers_t *sched_tick_irq(registers_t *regs);  // Handler de IRQ0 (Timer): realiza o context switch
registers_t *sched_yield_irq(registers_t *regs); // Handler de interrupção de cesso voluntário de CPU

/* Consulta de estado e estatísticas do Escalonador */
uint32_t sched_current_task(void);
uint32_t sched_current_pid(void);
uint32_t sched_switch_count(void);
uint32_t sched_task_count(void);
uint32_t sched_last_exit_pid(void);
uint32_t sched_current_ppid(void);
int32_t sched_last_exit_code(void);
int32_t task_wait_child(int32_t *status);

/* Criação de tarefas de Kernel (Ring 0) e Usuário (Ring 3) */
int sched_create_kernel_task(const char *name, void (*entry)(void));
int sched_create_user_task(const char *name, void (*entry)(void), uintptr_t user_stack_top);

/**
 * Cria uma tarefa em Ring 3 em um espaço de endereçamento (CR3) pré-configurado.
 * A posse do CR3 é transferida para o novo processo apenas em caso de sucesso.
 */
int sched_create_user_task_in_address_space(
    const char *name,
    uintptr_t entry,
    uintptr_t user_stack_top,
    uint32_t cr3
);

/* Reemplaza a imagem e o espaço de endereçamento do processo atual (mecanismo execve) */
int sched_exec_current_address_space(
    const char *name,
    uintptr_t entry,
    uintptr_t user_stack_top,
    uint32_t cr3,
    registers_t *regs
);

/* Funções e contadores de demonstração/testes do escalonador */
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

/* Diagnósticos e manipulação do ciclo de vida das Tarefas */
void sched_dump_tasks(void);
void task_list_tasks(void);
void task_sleep_ticks(uint32_t ticks);
void task_sleep_prepare(uint32_t ticks);

/* Gestão de Filas de Espera e Sincronização */
void wait_queue_init(wait_queue_t *queue);
int wait_queue_wake_one(wait_queue_t *queue);
void wait_queue_wake_all(wait_queue_t *queue);
void task_wait(wait_queue_t *queue);
int task_wait_until(wait_queue_t *queue, wait_condition_t condition, void *ctx);

/* Yield, Bloqueio e Finalização */
void task_yield(void);
void task_exit_code(int32_t exit_code) __attribute__((noreturn));
void task_exit(void) __attribute__((noreturn));
registers_t *task_exit_from_exception(registers_t *regs, int32_t exit_code);
void task_block(void);
void task_wake(task_t *task);

/* Acesso a ponteiros ativos */
task_t *sched_current_task_ptr(void);
process_t *sched_current_process_ptr(void);

#endif // KERNEL_TASK_H