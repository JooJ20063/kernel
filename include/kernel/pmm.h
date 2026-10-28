#pragma once

#include <stdint.h>

/* Tamanho padrão de um frame de memória física no x86 (4KB / 4096 bytes) */
#define PMM_FRAME_SIZE 4096U

/* Inicializações do alocador físico de memória (geralmente baseado em Bitmaps) */
void pmm_init(uint32_t memory_top_bytes);
void pmm_init_from_multiboot(uint32_t mb_info_addr, uintptr_t kernel_start, uintptr_t kernel_end);

/* Alocação e liberação padrão de frames de 4KB na memória RAM física */
uint32_t pmm_alloc_frame(void);
uint32_t pmm_alloc_frame_below(uint32_t limit_addr); // Aloca frame abaixo de um limite (ex: < 16MB para DMA)
uint32_t pmm_alloc_frame_above(uint32_t min_addr);   // Aloca frame acima de um limite especificado
void pmm_free_frame(uint32_t frame_addr);

/* Alocadores e identificadores de frames dedicados a estruturas de paginação (Page Tables / Page Directories) */
uint32_t pmm_alloc_table_frame(void);
void pmm_free_table_frame(uint32_t frame_addr);
uint8_t pmm_is_table_frame(uint32_t frame_addr);

/* Funções de telemetria e contagem de frames de memória física */
uint32_t pmm_free_frame_count(void);  // Retorna a quantidade de frames de 4KB livres
uint32_t pmm_total_frame_count(void); // Retorna a quantidade total de frames gerenciados no sistema