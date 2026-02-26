#pragma once

#include <stdint.h>
#include <kernel/vfs.h>

void init_ramfs(uintptr_t start, uintptr_t end);
fs_node_t *ramfs_root(void);
