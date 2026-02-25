#pragma once

#include <stdint.h>

void sched_init(uint32_t quantum_ticks);
void sched_tick(void);
uint32_t sched_current_task(void);
uint32_t sched_switch_count(void);
