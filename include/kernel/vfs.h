#pragma once

#include <stdint.h>

#define FS_FILE      0x01U
#define FS_DIRECTORY 0x02U

typedef struct fs_node fs_node_t;

typedef uint32_t (*read_type_t)(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
typedef uint32_t (*write_type_t)(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
typedef void (*open_type_t)(fs_node_t *node);
typedef void (*close_type_t)(fs_node_t *node);
typedef fs_node_t *(*readdir_type_t)(fs_node_t *node, uint32_t index);

struct fs_node {
    char name[128];
    uint32_t flags;
    uint32_t size;
    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;
    readdir_type_t readdir;
    void *device;
};

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
void open_fs(fs_node_t *node);
void close_fs(fs_node_t *node);
fs_node_t *readdir_fs(fs_node_t *node, uint32_t index);
