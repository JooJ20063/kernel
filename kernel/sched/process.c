#include <kernel/process.h>
#include <kernel/kmalloc.h>
#include <kernel/vmm.h>

static uint32_t next_pid = 1U;

void process_system_init(void) {
    next_pid = 1U;
}

void process_init_bootstrap(process_t *process, const char *name) {
    if (process == 0) {
        return;
    }

    process->pid = 0U;
    process->parent_pid = 0U;
    process->name = name;
    process->exit_code = 0;
    process->cr3 = 0U;
    process->exited = 0U;
    fd_table_init(&process->fds);
}

process_t *process_create(
    const char *name,
    uint32_t parent_pid,
    uint32_t cr3
) {
    process_t *process =
        (process_t *)kmalloc((uint32_t)sizeof(process_t));

    if (process == 0) {
        return 0;
    }

    process->pid = next_pid++;
    process->parent_pid = parent_pid;
    process->name = name;
    process->exit_code = 0;
    process->cr3 = cr3;
    process->exited = 0U;
    fd_table_init(&process->fds);

    return process;
}

void process_mark_exit(process_t *process, int32_t exit_code) {
    if (process == 0) {
        return;
    }

    process->exit_code = exit_code;

    if (process->exited == 0U) {
        fd_table_close_all(&process->fds);
        process->exited = 1U;
    }
}

void process_destroy(process_t *process) {
    if (process == 0) {
        return;
    }

    fd_table_close_all(&process->fds);
    vmm_destroy_address_space(process->cr3);
    kfree(process);
}
