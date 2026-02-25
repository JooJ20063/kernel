#include <kernel/sched.h>

#define TASK_COUNT 3

static uint32_t quantum = 10;
static uint32_t tick_acc;
static uint32_t current_task;
static uint32_t switches;

void sched_init(uint32_t quantum_ticks) {
    if (quantum_ticks != 0) {
        quantum = quantum_ticks;
    }

    tick_acc = 0;
    current_task = 0;
    switches = 0;
}

void sched_tick(void) {
    tick_acc++;

    if (tick_acc >= quantum) {
        tick_acc = 0;
        current_task = (current_task + 1) % TASK_COUNT;
        switches++;
    }
}

uint32_t sched_current_task(void) {
    return current_task;
}

uint32_t sched_switch_count(void) {
    return switches;
}
