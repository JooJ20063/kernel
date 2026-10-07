#include <kernel/fd.h>
#include <kernel/vfs.h>
#include <kernel/vga.h>

static void fd_entry_clear(fd_entry_t *entry) {
    entry->kind = FD_KIND_NONE;
    entry->node = 0;
    entry->offset = 0U;
    entry->access = 0U;
}

void fd_table_init(fd_table_t *table) {
    if (table == 0) {
        return;
    }

    for (uint32_t i = 0; i < FD_TABLE_MAX; ++i) {
        fd_entry_clear(&table->entries[i]);
    }

    table->entries[0].kind = FD_KIND_CONSOLE_IN;
    table->entries[0].access = FD_ACCESS_READ;

    table->entries[1].kind = FD_KIND_CONSOLE_OUT;
    table->entries[1].access = FD_ACCESS_WRITE;

    table->entries[2].kind = FD_KIND_CONSOLE_OUT;
    table->entries[2].access = FD_ACCESS_WRITE;
}

void fd_table_close_all(fd_table_t *table) {
    if (table == 0) {
        return;
    }

    for (uint32_t fd = 0; fd < FD_TABLE_MAX; ++fd) {
        fd_entry_t *entry = &table->entries[fd];

        if (entry->kind == FD_KIND_VFS && entry->node != 0) {
            close_fs(entry->node);
        }

        fd_entry_clear(entry);
    }
}

int fd_is_readable(const fd_table_t *table, uint32_t fd) {
    if (table == 0 || fd >= FD_TABLE_MAX) {
        return 0;
    }

    return table->entries[fd].kind != FD_KIND_NONE &&
           (table->entries[fd].access & FD_ACCESS_READ) != 0U;
}

int fd_is_writable(const fd_table_t *table, uint32_t fd) {
    if (table == 0 || fd >= FD_TABLE_MAX) {
        return 0;
    }

    return table->entries[fd].kind != FD_KIND_NONE &&
           (table->entries[fd].access & FD_ACCESS_WRITE) != 0U;
}

int32_t fd_open_vfs(fd_table_t *table, fs_node_t *node, uint32_t access) {
    if (table == 0 || node == 0 || access == 0U) {
        return -1;
    }

    if ((access & FD_ACCESS_READ) != 0U &&
        node->read == 0 &&
        node->readdir == 0) {
        return -2;
    }

    if ((access & FD_ACCESS_WRITE) != 0U &&
        (node->write == 0 || (node->flags & FS_WRITABLE) == 0U)) {
        return -2;
    }

    for (uint32_t fd = 3U; fd < FD_TABLE_MAX; ++fd) {
        fd_entry_t *entry = &table->entries[fd];

        if (entry->kind == FD_KIND_NONE) {
            entry->kind = FD_KIND_VFS;
            entry->node = node;
            entry->offset = 0U;
            entry->access = access;
            open_fs(node);
            return (int32_t)fd;
        }
    }

    return -1;
}

int32_t fd_close(fd_table_t *table, uint32_t fd) {
    fd_entry_t *entry;

    if (table == 0 || fd >= FD_TABLE_MAX) {
        return -1;
    }

    entry = &table->entries[fd];
    if (entry->kind == FD_KIND_NONE) {
        return -1;
    }

    if (entry->kind == FD_KIND_VFS && entry->node != 0) {
        close_fs(entry->node);
    }

    fd_entry_clear(entry);
    return 0;
}

int32_t fd_read(
    fd_table_t *table,
    uint32_t fd,
    uint8_t *buffer,
    uint32_t size
) {
    fd_entry_t *entry;
    uint32_t count;

    if (!fd_is_readable(table, fd) || (buffer == 0 && size != 0U)) {
        return -1;
    }

    if (size == 0U) {
        return 0;
    }

    entry = &table->entries[fd];

    if (entry->kind != FD_KIND_VFS || entry->node == 0) {
        return -1;
    }

    count = read_fs(entry->node, entry->offset, size, buffer);
    entry->offset += count;
    return (int32_t)count;
}

int32_t fd_write(
    fd_table_t *table,
    uint32_t fd,
    const uint8_t *buffer,
    uint32_t size
) {
    fd_entry_t *entry;

    if (!fd_is_writable(table, fd) || (buffer == 0 && size != 0U)) {
        return -1;
    }

    if (size == 0U) {
        return 0;
    }

    entry = &table->entries[fd];

    if (entry->kind == FD_KIND_CONSOLE_OUT) {
        for (uint32_t i = 0; i < size; ++i) {
            vga_putc((char)buffer[i]);
        }

        entry->offset += size;
        return (int32_t)size;
    }

    if (entry->kind == FD_KIND_VFS) {
        uint32_t written = write_fs(
            entry->node,
            entry->offset,
            size,
            buffer
        );

        entry->offset += written;
        return (int32_t)written;
    }

    return -1;
}

int32_t fd_seek(
    fd_table_t *table,
    uint32_t fd,
    int32_t offset,
    uint32_t whence,
    uint32_t *new_offset
) {
    fd_entry_t *entry;
    int64_t base;
    int64_t result;

    if (table == 0 || fd >= FD_TABLE_MAX || new_offset == 0) {
        return -1;
    }

    entry = &table->entries[fd];

    if (entry->kind == FD_KIND_NONE) {
        return -1;
    }

    if (entry->kind != FD_KIND_VFS || entry->node == 0) {
        return -2;
    }

    switch (whence) {
        case 0U:
            base = 0;
            break;

        case 1U:
            base = (int64_t)entry->offset;
            break;

        case 2U:
            base = (int64_t)entry->node->size;
            break;

        default:
            return -3;
    }

    result = base + (int64_t)offset;

    if (result < 0 || (uint64_t)result > 0xFFFFFFFFULL) {
        return -3;
    }

    entry->offset = (uint32_t)result;
    *new_offset = entry->offset;
    return 0;
}

int32_t fd_stat(
    fd_table_t *table,
    uint32_t fd,
    uint32_t *size_out,
    uint32_t *flags_out
) {
    fd_entry_t *entry;

    if (table == 0 ||
        fd >= FD_TABLE_MAX ||
        size_out == 0 ||
        flags_out == 0) {
        return -1;
    }

    entry = &table->entries[fd];

    if (entry->kind != FD_KIND_VFS || entry->node == 0) {
        return -1;
    }

    *size_out = entry->node->size;
    *flags_out = entry->node->flags;
    return 0;
}

int32_t fd_readdir(
    fd_table_t *table,
    uint32_t fd,
    fs_node_t **node_out
) {
    fd_entry_t *entry;
    fs_node_t *node;

    if (table == 0 ||
        fd >= FD_TABLE_MAX ||
        node_out == 0) {
        return -1;
    }

    entry = &table->entries[fd];

    if (entry->kind == FD_KIND_NONE || entry->node == 0) {
        return -1;
    }

    if ((entry->node->flags & FS_DIRECTORY) == 0U ||
        entry->node->readdir == 0) {
        return -2;
    }

    node = readdir_fs(entry->node, entry->offset);
    if (node == 0) {
        return 0;
    }

    entry->offset++;
    *node_out = node;
    return 1;
}
