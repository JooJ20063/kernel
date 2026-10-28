#pragma once

#include <kernel/vfs.h>

/* Inicializa o sistema de arquivos virtual de dispositivos (DevFS) e seus nós (/dev) */
int devfs_init(void);

/* Retorna o nó de arquivo (fs_node_t) que representa o diretório raiz do DevFS */
fs_node_t *devfs_root(void);