#pragma once

#include <stdint.h>
#include <kernel/vfs.h>

/**
 * Inicializa o RAMFS montando a imagem pré-carregada na memória RAM física/virtual
 * delimitada pelos endereços 'start' e 'end' (ex: módulo de boot do Multiboot2).
 */
void init_ramfs(uintptr_t start, uintptr_t end);

/* Retorna o nó de arquivo (fs_node_t) correspondente ao diretório raiz ("/") do RAMFS */
fs_node_t *ramfs_root(void);

/* Busca e retorna o nó do VFS associado ao caminho de arquivo/diretório especificado */
fs_node_t *ramfs_find(const char *path);

/* Cria um novo arquivo vazio no RAMFS no caminho especificado (ou atualiza metadados se já existir) */
fs_node_t *ramfs_touch(const char *path);

/* Cria um novo nó de diretório no RAMFS no caminho especificado */
fs_node_t *ramfs_mkdir(const char *path);