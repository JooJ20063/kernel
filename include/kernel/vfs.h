#pragma once

#include <stdint.h>

#define FS_FILE      0x01U
#define FS_DIRECTORY 0x02U
#define FS_WRITABLE  0x04U

#define VFS_NAME_MAX 128U

typedef struct fs_node fs_node_t;

typedef uint32_t (*read_type_t)(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
typedef uint32_t (*write_type_t)(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
typedef void (*open_type_t)(fs_node_t *node);
typedef void (*close_type_t)(fs_node_t *node);
typedef fs_node_t *(*readdir_type_t)(fs_node_t *node, uint32_t index);
typedef fs_node_t *(*finddir_type_t)(fs_node_t *node, const char *name);
typedef fs_node_t *(*create_type_t)(fs_node_t *node, const char *name, uint32_t flags);

struct fs_node {
    char name[VFS_NAME_MAX];
    uint32_t flags;
    uint32_t size;
    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;
    readdir_type_t readdir;
    finddir_type_t finddir;
    create_type_t create;
    void *device;
};

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
void open_fs(fs_node_t *node);
void close_fs(fs_node_t *node);
fs_node_t *readdir_fs(fs_node_t *node, uint32_t index);
fs_node_t *finddir_fs(fs_node_t *node, const char *name);
fs_node_t *create_fs(fs_node_t *node, const char *name, uint32_t flags);

void vfs_set_root(fs_node_t *root);
fs_node_t *vfs_root(void);
fs_node_t *vfs_resolve(const char *path);
fs_node_t *vfs_create(const char *path, uint32_t flags);

int vfs_mount(const char *path, fs_node_t *root);
int vfs_unmount(const char *path);
uint32_t vfs_mount_count(void);
