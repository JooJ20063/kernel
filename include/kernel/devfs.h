#pragma once

#include <kernel/vfs.h>

int devfs_init(void);
fs_node_t *devfs_root(void);
