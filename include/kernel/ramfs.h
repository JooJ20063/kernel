#pragma once

#include <stdint.h>
#include <kernel/vfs.h>

void init_ramfs(uintptr_t start, uintptr_t end);
fs_node_t *ramfs_root(void);
fs_node_t *ramfs_find(const char *path);
fs_node_t *ramfs_touch(const char *path);
fs_node_t *ramfs_mkdir(const char *path);
