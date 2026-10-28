#pragma once
#include <stdint.h>

/* Inicializa as estruturas de controle e o espaço de endereçamento do Heap do Kernel */
void kmalloc_init(void);

/* Aloca um bloco contíguo de memória dinâmica no Heap do Kernel de pelo menos 'size' bytes */
void *kmalloc(uint32_t size);

/* Libera o bloco de memória apontado por 'ptr' de volta para a reserva do Heap do Kernel */
void kfree(void *ptr);

/* Redimensiona um bloco de memória alocado anteriormente, preservando seu conteúdo original */
void *krealloc(void *ptr, uint32_t size);

/* Funções de telemetria: Retornam métricas em bytes e contagem de blocos do Heap */
uint32_t kmalloc_bytes_used(void);   // Total de memória atualmente alocada
uint32_t kmalloc_bytes_free(void);   // Memória disponível para novas alocações
uint32_t kmalloc_bytes_mapped(void); // Limite total do espaço de endereçamento mapeado para o Heap
uint32_t kmalloc_block_count(void);  // Quantidade de blocos gerenciados na estrutura do Heap

/* Executa uma verificação de sanidade na lista/árvore de blocos do Heap para detectar corrupção de memória */
uint8_t kheap_check(void);