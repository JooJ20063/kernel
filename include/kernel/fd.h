#ifndef KERNEL_FD_H
#define KERNEL_FD_H

#include <stdint.h>

#define FD_TABLE_MAX 16U

#define FD_ACCESS_READ  0x01U
#define FD_ACCESS_WRITE 0x02U

typedef struct fs_node fs_node_t;

typedef enum fd_kind {
    FD_KIND_NONE = 0,
    FD_KIND_CONSOLE_IN,
    FD_KIND_CONSOLE_OUT,
    FD_KIND_VFS
} fd_kind_t;

typedef struct fd_entry {
    fd_kind_t kind;
    fs_node_t *node;
    uint32_t offset;
    uint32_t access;
} fd_entry_t;

typedef struct fd_table {
    fd_entry_t entries[FD_TABLE_MAX];
} fd_table_t;

void fd_table_init(fd_table_t *table);
void fd_table_close_all(fd_table_t *table);

int fd_is_readable(const fd_table_t *table, uint32_t fd);
int fd_is_writable(const fd_table_t *table, uint32_t fd);

int32_t fd_open_vfs(fd_table_t *table, fs_node_t *node, uint32_t access);
int32_t fd_close(fd_table_t *table, uint32_t fd);
int32_t fd_read(fd_table_t *table, uint32_t fd, uint8_t *buffer, uint32_t size);
int32_t fd_write(fd_table_t *table, uint32_t fd, const uint8_t *buffer, uint32_t size);

#endif
