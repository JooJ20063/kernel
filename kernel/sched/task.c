#include <kernel/task.h>
#include <arch/x86/fpu.h>
#include <kernel/kmalloc.h>
#include <kernel/vga.h>
#include <arch/x86/irq.h>
#include <arch/x86/tss.h>

#define KERNEL_STACK_SIZE   4096
#define EFLAGS_IF           0x00000202U
#define KERNEL_CS           0x08U
#define KERNEL_DS           0x10U
#define USER_CS             0x1BU
#define USER_DS             0x23U

static uint32_t quantum = 10; // Default quantum in ticks
static uint32_t tick_acc = 0;
static uint32_t switches = 0;
static uint32_t next_pid = 1; // Start PID from 1
static uint32_t task_count = 0;
static uint32_t last_exit_pid = 0;
static uint32_t last_exit_code = 0;

static uint8_t demo_started = 0;

static task_t idle_task;
static task_t *current = 0;
static task_t *task_list = 0;

static volatile uint32_t demo_counter_a = 0;
static volatile uint32_t demo_counter_b = 0;



static volatile float sse_value_a = 0.0f;
static volatile float sse_value_b = 0.0f;

static void mem_zero(void *ptr, uint32_t size) {
    uint8_t *p = (uint8_t *)ptr;
    
    for (uint32_t i = 0; i < size; i++) {
        p[i] = 0;
    }
}

static const char *state_name(task_state_t state) {
    switch (state) {
        case TASK_UNUSED:   return "UNUSED";
        case TASK_READY:    return "READY";
        case TASK_RUNNING:  return "RUNNING";
        case TASK_BLOCKED:  return "BLOCKED";
        case TASK_ZOMBIE:   return "ZOMBIE";
        default:           return "UNKNOWN";
    }
}

static inline uint32_t irq_save_disable(void) {
    uint32_t flags;

    asm volatile (
        "pushf\n"
        "pop %0\n"
        "cli"
        : "=r"(flags)
        :
        : "memory"
    );

    return flags;
}

static inline void irq_restore(uint32_t flags) {
    if (flags & (1U << 9)) {
        asm volatile ("sti" : : : "memory");
    }
}

static void add_task(task_t *task) {
    if (task_list == 0) {
        task_list = task;
        task->next = task;
    } else {
        task_t *tail = task_list;

        while (tail->next != task_list) {
            tail = tail->next;
        }

        tail->next = task;
        task->next = task_list;
    }
    
    task_count++;
}

static void wake_sleeping_tasks(void) {
    if (task_list == 0) {
        return;
    }

    uint32_t now = irq_timer_ticks();
    task_t *t = task_list;

    do {
        if (t->state == TASK_BLOCKED &&
            t->block_reason == TASK_BLOCK_SLEEP &&
            (int32_t)(now - t->wake_tick) >= 0) {

            t->state = TASK_READY;
            t->block_reason = TASK_BLOCK_NONE;
            t->wake_tick = 0;
        }

        t = t->next;
    } while (t != task_list);
}

static void update_tss_for_task(task_t *task) {
    if (task == 0 ||
        task->kernel_stack == 0 ||
        task->kernel_stack_size == 0) {
        return;
    }

    uint32_t stack_top =
        (uint32_t)(uintptr_t)task->kernel_stack +
        task->kernel_stack_size;

    tss_set_kernel_stack(stack_top);
}

static task_t *pic_next_ready(void) {
    if (current == 0) {
        return 0;
    }

    task_t *start = current->next;
    task_t *t = start;

    do {
        if (t->state == TASK_READY) {
            return t;
        }
        t = t->next;
    } while (t != start);

    return current;
}

void wait_queue_init(wait_queue_t *queue) {
    if (queue == 0) {
        return;
    }

    queue->head = 0;
    queue->tail = 0;
}

void sched_init(uint32_t quantum_ticks) {
    if (quantum_ticks > 0) {
        quantum = quantum_ticks;
    }

    tick_acc = 0;
    switches = 0;
    next_pid = 1;
    task_count = 0;
    task_list = 0;

    mem_zero(&idle_task, sizeof(idle_task));

    idle_task.pid = 0;
    idle_task.parent_pid = 0;
    idle_task.state = TASK_RUNNING;
    idle_task.name = "idle";
    idle_task.context = 0;
    idle_task.kernel_stack = 0;
    idle_task.kernel_stack_size = 0;
    idle_task.next = 0;
    idle_task.cr3 = 0;
    idle_task.fpu_storage = 0;
    idle_task.fpu_area = 0;
    idle_task.fpu_initialized = 0;

    add_task(&idle_task);

    current = &idle_task;
}

void task_sleep_ticks(uint32_t ticks) {
    if (current == 0 || current == &idle_task) {
        return;
    }

    if (ticks == 0) {
        task_yield();
        return;
    }

    current->wake_tick = irq_timer_ticks() + ticks;
    current->block_reason = TASK_BLOCK_SLEEP;
    current->state = TASK_BLOCKED;

    task_yield();
}

void task_sleep_prepare(uint32_t ticks) {
    if (current == 0 || current == &idle_task) {
        return;
    }

    if (ticks == 0) {
        return;
    }

    current->wake_tick = irq_timer_ticks() + ticks;
    current->block_reason = TASK_BLOCK_SLEEP;
    current->state = TASK_BLOCKED;
}

void task_wait(wait_queue_t *queue) {
    uint32_t flags;

    if (queue == 0 ||
        current == 0 ||
        current == &idle_task) {
        return;
    }

    flags = irq_save_disable();

    current->wait_next = 0;
    current->wake_tick = 0;
    current->block_reason = TASK_BLOCK_EVENT;
    current->state = TASK_BLOCKED;

    if (queue->tail == 0) {
        queue->head = current;
        queue->tail = current;
    } else {
        queue->tail->wait_next = current;
        queue->tail = current;
    }

    /*
     * Se IF estava ligado antes do CLI:
     *
     * STI só permite IRQ externa depois da instrução seguinte.
     * A instrução seguinte é justamente INT $0x81.
     *
     * Portanto não existe janela entre desbloquear IRQ e entrar
     * no scheduler.
     */
    if (flags & (1U << 9)) {
        asm volatile (
            "sti\n"
            "int $0x81"
            :
            :
            : "memory"
        );
    } else {
        asm volatile (
            "int $0x81"
            :
            :
            : "memory"
        );
    }
}

int wait_queue_wake_one(wait_queue_t *queue) {
    uint32_t flags;
    task_t *task;

    if (queue == 0) {
        return 0;
    }

    flags = irq_save_disable();

    task = queue->head;

    if (task == 0) {
        irq_restore(flags);
        return 0;
    }

    queue->head = task->wait_next;

    if (queue->head == 0) {
        queue->tail = 0;
    }

    task->wait_next = 0;
    task_wake(task);

    irq_restore(flags);

    return 1;
}

void wait_queue_wake_all(wait_queue_t *queue) {
    uint32_t flags;
    task_t *task;

    if (queue == 0) {
        return;
    }

    flags = irq_save_disable();

    task = queue->head;

    queue->head = 0;
    queue->tail = 0;

    while (task != 0) {
        task_t *next = task->wait_next;

        task->wait_next = 0;
        task_wake(task);

        task = next;
    }

    irq_restore(flags);
}

void task_block(void) {
    if (current == 0 || current == &idle_task) {
        return;
    }

    current->wake_tick = 0;
    current->block_reason = TASK_BLOCK_EVENT;
    current->state = TASK_BLOCKED;

    task_yield();
}

void task_wake(task_t *task) {
    if (task == 0) {
        return;
    }

    if (task->state != TASK_BLOCKED) {
        return;
    }

    task->wake_tick = 0;
    task->block_reason = TASK_BLOCK_NONE;
    task->state = TASK_READY;
}

static task_t *find_task_by_pid(uint32_t pid) {
    if (task_list == 0) {
        return 0;
    }

    task_t *t = task_list;

    do {
        if (t->pid == pid) {
            return t;
        }

        t = t->next;
    } while (t != task_list);

    return 0;
}

int32_t task_wait_child(int32_t *status) {
    if (current == 0 || task_list == 0) {
        return -1;
    }

    uint32_t parent_pid = current->pid;

    task_t *prev = task_list;
    task_t *t = task_list->next;

    do {
        if (t->parent_pid == parent_pid &&
            t->state == TASK_ZOMBIE &&
            t != &idle_task &&
            t != current) {

            uint32_t pid = t->pid;
            int32_t code = t->exit_code;

            prev->next = t->next;

            if (status != 0) {
                *status = code;
            }

            last_exit_pid = pid;
            last_exit_code = code;

            if (t->fpu_storage != 0) {
                kfree(t->fpu_storage);
            }

            if (t->kernel_stack != 0) {
                kfree(t->kernel_stack);
            }

            kfree(t);

            if (task_count > 0) {
                task_count--;
            }

            return (int32_t)pid;
        }

        prev = t;
        t = t->next;

    } while (t != task_list);

    return -1;
}

static void reap_zombies(void) {
    if (task_list == 0) {
        return;
    }

    task_t *prev = task_list;
    task_t *t = task_list->next;


    do {
        task_t *parent = find_task_by_pid(t->parent_pid);

        if (t->state == TASK_ZOMBIE &&
            t != current &&
            t != &idle_task &&
            (parent == 0 ||
            parent->state == TASK_ZOMBIE)) {

            prev->next = t->next;

            task_t *dead = t;
            t = t->next;

            last_exit_pid = dead->pid;
            last_exit_code = dead->exit_code;

            if (dead->fpu_storage != 0) {
                kfree(dead->fpu_storage);
            }

            if (dead->kernel_stack != 0) {
                kfree(dead->kernel_stack);
            }

            kfree(dead);

            if (task_count > 0) {
                task_count--;
            }

            continue;
        }

        prev = t;
        t = t->next;

    } while (t != task_list);
}


registers_t *sched_tick_irq(registers_t *regs) {
    if (current == 0) {
        return regs;
    }

    reap_zombies();
    wake_sleeping_tasks();

    current->context = regs;

    tick_acc++;

    if (tick_acc < quantum) {
        return regs;
    }

    tick_acc = 0;

    task_t *old = current;
    task_t *next = pic_next_ready();

    if (next == 0 || next == old || next->context == 0) {
        return regs;
    }

    if (old->state == TASK_RUNNING) {
        old->state = TASK_READY;
    }

    next->state = TASK_RUNNING;
    current = next;
    switches++;

    update_tss_for_task(next);
    fpu_set_ts();

    return next->context;
}

registers_t *sched_yield_irq(registers_t *regs) {
    if (current == 0) {
        return regs;
    }

    current->context = regs;

    task_t *old = current;
    task_t *next = pic_next_ready();

    if (next ==  0 || next == old || next->context == 0) {
        return regs;
    }

    if (old->state == TASK_RUNNING) {
        old->state = TASK_READY;
    }

    next->state = TASK_RUNNING;
    current = next;
    switches++;

    update_tss_for_task(next);
    fpu_set_ts();

    return next->context;
}

uint32_t sched_current_ppid(void){
    if (current == 0) {
        return 0;
    }
    return current->parent_pid;
}

int sched_create_kernel_task(const char *name, void (*entry)(void)) {
    if (entry == 0) {
        return -1;
    }

    task_t *task = (task_t *)kmalloc(sizeof(task_t));

    if (task == 0) {
        return -2;
    }
    
    uint8_t *stack = (uint8_t *)kmalloc(KERNEL_STACK_SIZE);

    if (stack == 0) {
        return -3;
    }

    mem_zero(task, sizeof(task_t));
    mem_zero(stack, KERNEL_STACK_SIZE);

    uintptr_t top = (uintptr_t)stack + KERNEL_STACK_SIZE;
    top -= sizeof(registers_t);

    registers_t *frame = (registers_t *)top;
    mem_zero(frame, sizeof(registers_t));

    frame->gs = KERNEL_DS;
    frame->fs = KERNEL_DS;
    frame->es = KERNEL_DS;
    frame->ds = KERNEL_DS;

    frame->eip = (uint32_t)(uintptr_t)entry;
    frame->cs = KERNEL_CS;
    frame->eflags = EFLAGS_IF;

    task->pid = next_pid++;
    task->parent_pid = (current != 0) ? current->pid : 0;
    task->name = name;
    task->state = TASK_READY;
    task->context = frame;
    task->kernel_stack = stack;
    task->kernel_stack_size = KERNEL_STACK_SIZE;
    add_task(task);

    task->cr3 = 0;
    fpu_init_task(task);

    return (int)task->pid;
}

int sched_create_user_task(
    const char *name,
    void (*entry)(void),
    uintptr_t user_stack_top
) {
    if (entry == 0 || user_stack_top == 0U) {
        return -1;
    }

    task_t *task = (task_t *)kmalloc(sizeof(task_t));
    if (task == 0) {
        return -2;
    }

    uint8_t *stack = (uint8_t *)kmalloc(KERNEL_STACK_SIZE);
    if (stack == 0) {
        kfree(task);
        return -3;
    }

    mem_zero(task, sizeof(task_t));
    mem_zero(stack, KERNEL_STACK_SIZE);

    uintptr_t top = (uintptr_t)stack + KERNEL_STACK_SIZE;
    top -= sizeof(registers_t);

    registers_t *frame = (registers_t *)top;
    mem_zero(frame, sizeof(registers_t));

    /*
     * Segmentos restaurados pelo irq_common_stub antes do iret.
     * Precisam ser seletores DPL3.
     */
    frame->gs = USER_DS;
    frame->fs = USER_DS;
    frame->es = USER_DS;
    frame->ds = USER_DS;

    /*
     * Estado inicial em CPL3.
     */
    frame->eip = (uint32_t)(uintptr_t)entry;
    frame->cs = USER_CS;
    frame->eflags = EFLAGS_IF;

    /*
     * Como o iret troca CPL0 -> CPL3, ele também consome
     * ESP e SS de usuário.
     */
    frame->useresp = (uint32_t)user_stack_top;
    frame->ss = USER_DS;

    task->pid = next_pid++;
    task->parent_pid = (current != 0) ? current->pid : 0;
    task->name = name;
    task->state = TASK_READY;
    task->context = frame;

    /*
     * Esta é a kernel stack usada quando uma IRQ/syscall
     * entra no kernel a partir dessa user task.
     */
    task->kernel_stack = stack;
    task->kernel_stack_size = KERNEL_STACK_SIZE;

    /*
     * Por enquanto user/kernel compartilham o mesmo CR3.
     * Depois criaremos address spaces por processo.
     */
    task->cr3 = 0;

    add_task(task);
    fpu_init_task(task);

    return (int)task->pid;
}

static void demo_task_a(void) {
    float x = 1.0f;

    for (;;) {
        x += 0.25f;
        sse_value_a = x;
    }
}

static void demo_task_b(void) {
    float x = 1.0f;

    for (;;) {
        x += 0.25f;
        sse_value_b = x;
    }
}

static volatile uint32_t exit_task_run = 0;

static void demo_exit_task(void) {
    exit_task_run = 1;
    task_exit();
}

static wait_queue_t event_demo_queue;
static volatile uint32_t event_a_counter = 0;
static volatile uint32_t event_b_counter = 0;
static void demo_event_a(void) {
    for (;;) {
        event_a_counter++;
        task_wait(&event_demo_queue);
    }
}

static void demo_event_b(void) {
    for (;;) {
        event_b_counter++;
        task_wait(&event_demo_queue);
    }
}

static volatile uint32_t yield_counter = 0;

static void demo_task_yield(void) {
    for (;;) {
        yield_counter++;
        task_yield();
    }
}

static volatile uint32_t sleep_demo_counter = 0;

static void demo_sleep_task(void) {
    for (;;) {
        sleep_demo_counter++;
        task_sleep_ticks(100);
    }
}

static volatile uint32_t event_demo_counter = 0;
static volatile uint32_t event_waker_counter = 0;


static void demo_event_waker(void) {
    for (;;) {
        task_sleep_ticks(300);

        wait_queue_wake_all(&event_demo_queue);
        event_waker_counter++;
        
    }
}

void sched_demo_init(void) {
    if (demo_started) {
        return;
    }

    demo_started = 1;

    demo_counter_a = 0;
    demo_counter_b = 0;
    yield_counter = 0;
    exit_task_run = 0;
    sleep_demo_counter = 0;
    event_demo_counter = 0;
    event_waker_counter = 0;

    wait_queue_init(&event_demo_queue);
    (void)sched_create_kernel_task("yield-demo", demo_task_yield);
    (void)sched_create_kernel_task("demo-a", demo_task_a);
    (void)sched_create_kernel_task("demo-b", demo_task_b);
    (void)sched_create_kernel_task("exit-demo", demo_exit_task);
    (void)sched_create_kernel_task("sleep-demo", demo_sleep_task);
    (void)sched_create_kernel_task("event-waker", demo_event_waker);
    (void)sched_create_kernel_task("event-a", demo_event_a);
    (void)sched_create_kernel_task("event-b", demo_event_b);
}

uint32_t sched_demo_counter_a(void) {
    return demo_counter_a;
}

uint32_t sched_demo_counter_b(void) {
    return demo_counter_b;
}

uint32_t sched_event_a_counter(void) {
    return event_a_counter;
}

uint32_t sched_event_b_counter(void) {
    return event_b_counter;
}

uint32_t sched_yield_counter(void) {
    return yield_counter;
}

uint32_t sched_event_demo_counter(void) {
    return event_demo_counter;
}

uint32_t sched_event_waker_counter(void) {
    return event_waker_counter;
}

uint32_t sched_last_exit_pid(void) {
    return last_exit_pid;
}

int32_t sched_last_exit_code(void) {
    return last_exit_code;
}

uint32_t sched_current_task(void) {
    if (current == 0) {
        return 0;
    }
    return current->pid;
}

uint32_t sched_exit_task_run(void) {
    return exit_task_run;
}

uint32_t sched_sleep_demo_counter(void) {
    return sleep_demo_counter;
}

uint32_t sched_current_pid(void) {
    if (current == 0) {
        return 0;
    }
    return current->pid;
}

uint32_t sched_switch_count(void) {
    return switches;
}

uint32_t sched_task_count(void) {
    return task_count;
}

void task_list_tasks(void) {
    if (task_list == 0) {
        vga_puts("No tasks");
        return;
    }

    vga_puts("PID   PPID   Name          State     Stack\n");
    vga_puts("-------------------------------------------------\n");
    
    task_t *t = task_list;

    do {
        vga_putdec(t->pid);
        vga_puts("   ");
        vga_putdec(t->parent_pid);
        vga_puts("   ");


        if (t->name != 0) {
            vga_puts(t->name);
        } else {
            vga_puts("unnamed");
        }

        vga_puts("   ");
        vga_puts(state_name(t->state));

        vga_puts("   ");

        if (t->kernel_stack != 0) {
            vga_puthex((uint32_t)(uintptr_t)t->kernel_stack);
        } else {
            vga_puts("NULL");
        }

        vga_puts("\n");

        t = t->next;
    } while (t != task_list);
}

void sched_dump_tasks(void) {
    task_list_tasks();
}

void task_yield(void) {
    asm volatile ("int $0x81");
}

void task_exit_code(int32_t code) {
    if (current != 0) {
        current->exit_code = code;
        current->block_reason = TASK_BLOCK_NONE;
        current->wake_tick = 0;
        current->state = TASK_ZOMBIE;
    }

    for (;;) {
        task_yield();
    }
}

void task_exit(void) {
    task_exit_code(0);
}

task_t *sched_current_task_ptr(void) {
    return current;
}

uint32_t sched_sse_value_a(void) {
    return (uint32_t)sse_value_a;
}

uint32_t sched_sse_value_b(void) {
    return (uint32_t)sse_value_b;
}