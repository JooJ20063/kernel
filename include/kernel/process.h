#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <stdint.h>
#include <kernel/fd.h>

typedef struct process_struct {
    uint32_t pid;
    uint32_t parent_pid;
    const char *name;

    int32_t exit_code;
    uint32_t cr3;
    uint8_t exited;

    fd_table_t fds;
} process_t;

void process_system_init(void);
void process_init_bootstrap(process_t *process, const char *name);
process_t *process_create(const char *name, uint32_t parent_pid, uint32_t cr3);
void process_mark_exit(process_t *process, int32_t exit_code);
void process_destroy(process_t *process);

#endif
