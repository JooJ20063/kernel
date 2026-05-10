#include <kernel/task.h>
#include <arch/x86/fpu.h>
#include <kernel/kmalloc.h>
#include <kernel/vga.h>

#define KERNEL_STACK_SIZE   4096
#define EFLAGS_IF           0x00000202U
#define KERNEL_CS           0x08U
#define KERNEL_DS           0x10U

static uint32_t quantum = 10; // Default quantum in ticks
static uint32_t tick_acc = 0;
static uint32_t switches = 0;
static uint32_t next_pid = 1; // Start PID from 1
static uint32_t task_count = 0;

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
    idle_task.state = TASK_RUNNING;
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

registers_t *sched_tick_irq(registers_t *regs) {
    if (current == 0) {
        return regs;
    }

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

    fpu_set_ts();

    return next->context;
}

int sched_create_kernel_task(void (*entry)(void)) {
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
    task->state = TASK_READY;
    task->context = frame;
    task->kernel_stack = stack;
    task->kernel_stack_size = KERNEL_STACK_SIZE;
    add_task(task);

    task->cr3 = 0;
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

void sched_demo_init(void) {
    demo_counter_a = 0;
    demo_counter_b = 0;

    (void)sched_create_kernel_task(demo_task_a);
    (void)sched_create_kernel_task(demo_task_b);
}

uint32_t sched_demo_counter_a(void) {
    return demo_counter_a;
}

uint32_t sched_demo_counter_b(void) {
    return demo_counter_b;
}

uint32_t sched_current_task(void) {
    if (current == 0) {
        return 0;
    }
    return current->pid;
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

    vga_puts("PID   State     Stack");
    vga_puts("-----------------------------");

    task_t *t = task_list;

    do {
        vga_putdec(t->pid);
        vga_puts("   ");
        vga_puts(state_name(t->state));
        vga_puts("   0x");

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
    asm volatile ("int $32");
}

void task_exit(void) {
    if (current == 0) {
        current->state = TASK_ZOMBIE;
    }

    for (;;) {
        task_yield();
    }
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