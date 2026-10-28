#pragma once

#include <stdint.h>

/* Tipos de Tags da especificação Multiboot2 */
#define MULTIBOOT2_TAG_END    0  // Marca o fim da lista de tags passadas pelo bootloader
#define MULTIBOOT2_TAG_MODULE 3  // Módulo de arquivo carregado na RAM (ex: ramdisk/initrd)
#define MULTIBOOT2_TAG_MMAP   6  // Tabela de mapa de memória física (Memory Map)

/* Tipo de região de memória no mapa Multiboot2 */
#define MULTIBOOT2_MMAP_AVAILABLE 1  // RAM física livre para uso pelo sistema operacional

/**
 * Cabeçalho genérico de uma Tag Multiboot2 (8 bytes).
 * Todas as tags começam com esta estrutura e são alinhadas em limites de 8 bytes na memória.
 */
struct multiboot2_tag {
    uint32_t type;  // Identificador do tipo da tag
    uint32_t size;  // Tamanho total da tag em bytes (incluindo este cabeçalho)
} __attribute__((packed));

/**
 * Entrada individual do mapa de memória física (24 bytes).
 */
struct multiboot2_mmap_entry {
    uint64_t addr;  // Endereço físico inicial da região
    uint64_t len;   // Tamanho da região de memória em bytes
    uint32_t type;  // Tipo da região (ex: 1 = RAM disponível, outros = reservado/ACPI)
    uint32_t zero;  // Campo reservado pela especificação
} __attribute__((packed));

/**
 * Tag que descreve um módulo carregado na memória pelo bootloader (ex: initrd).
 */
struct multiboot2_tag_module {
    uint32_t type;       // MULTIBOOT2_TAG_MODULE (3)
    uint32_t size;       // Tamanho total da tag + string de comando
    uint32_t mod_start;  // Endereço físico inicial do módulo na RAM
    uint32_t mod_end;    // Endereço físico final do módulo na RAM
    char cmdline[];      // Flexible Array Member: String de linha de comando do módulo (terminada em null)
} __attribute__((packed));

/**
 * Tag contendo a tabela completa com o mapa da memória física RAM detectada pelo BIOS/UEFI.
 */
struct multiboot2_tag_mmap {
    uint32_t type;           // MULTIBOOT2_TAG_MMAP (6)
    uint32_t size;           // Tamanho total da tag
    uint32_t entry_size;     // Tamanho de cada estrutura multiboot2_mmap_entry (geralmente 24 bytes)
    uint32_t entry_version;  // Versão das entradas do mapa (definida como 0)
    struct multiboot2_mmap_entry entries[]; // Array flexível contendo as regiões de memória
} __attribute__((packed));