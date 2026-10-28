#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include <stdint.h>
#include <kernel/fd.h>

/**
 * Estrutura do Bloco de Controle de Processo (PCB - Process Control Block).
 * Contém o estado de execução, identificadores, contexto de memória e tabela de arquivos.
 */
typedef struct process_struct {
    uint32_t pid;          // Identificador único do processo (Process ID)
    uint32_t parent_pid;   // PID do processo pai (para sinalização e códigos de retorno do wait)
    const char *name;      // Nome do executável/processo para identificação no sistema

    int32_t exit_code;     // Código de término retornado pela chamada exit()
    uint32_t cr3;          // Endereço físico da página do Page Directory (espaço de endereçamento virtual)
    uint8_t exited;        // Flag indicando se o processo foi finalizado (status Zombie/Terminado)

    fd_table_t fds;        // Tabela de descritores de arquivos abertos isolada por processo
} process_t;

/* Inicializa o subsistema de gerenciamento de processos do Kernel */
void process_system_init(void);

/* Configura o processo inicial/bootstrap do kernel (Processo Kernel/Idle PID 0) */
void process_init_bootstrap(process_t *process, const char *name);

/* Aloca e inicializa uma nova PCB atribuindo um PID, processo pai e novo diretório de páginas (CR3) */
process_t *process_create(const char *name, uint32_t parent_pid, uint32_t cr3);

/* Marca o processo como encerrado e registra seu código de saída para o processo pai */
void process_mark_exit(process_t *process, int32_t exit_code);

/* Destrói a estrutura PCB, fecha todos os seus descritores de arquivo e libera a memória associada */
void process_destroy(process_t *process);

#endif