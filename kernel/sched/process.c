#include <kernel/process.h>
#include <kernel/kmalloc.h>
#include <kernel/vmm.h>
#include <kernel/vfs.h>

/* Contador global estático para atribuição sequencial de PIDs */
static uint32_t next_pid = 1U;

/* Reseta o contador sequencial de PIDs para a inicialização do subsistema */
void process_system_init(void) {
    next_pid = 1U;
}

/**
 * Inicializa a estrutura de processo do Bootstrap/Idle (PID 0).
 * Executa em Ring 0 e utiliza o CR3 padrão do Kernel sem alocar novos recursos.
 */
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

/**
 * Aloca e inicializa uma nova estrutura de processo (process_t) no Heap do Kernel.
 * Atribui um PID único, associa o espaço de endereçamento (CR3) e vincula
 * automaticamente os descritores padrão (stdin, stdout, stderr) ao nó /dev/tty1.
 */
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

    /* Vincula automaticamente os descritores de stdio ao terminal /dev/tty1 */
    {
        fs_node_t *tty = vfs_resolve("/dev/tty1");

        if (tty == 0 ||
            fd_table_bind_stdio(&process->fds, tty) != 0) {
            kfree(process);
            return 0;
        }
    }

    return process;
}

/**
 * Marca o processo como encerrado, registra o código de término (exit_code)
 * e fecha todos os descritores de arquivos ativos na tabela de FDs.
 */
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

/**
 * Destrói completamente um processo liberando todos os seus recursos:
 * fecha a tabela de descritores, destrói o espaço de memória virtual no VMM (CR3)
 * e libera a estrutura no heap do kernel via kfree.
 */
void process_destroy(process_t *process) {
    if (process == 0) {
        return;
    }

    fd_table_close_all(&process->fds);
    vmm_destroy_address_space(process->cr3);
    kfree(process);
}