#include <kernel/fd.h>
#include <kernel/vfs.h>
#include <kernel/vga.h>

static void fd_entry_clear(fd_entry_t *entry) {
    entry->kind = FD_KIND_NONE;
    entry->node = 0;
    entry->offset = 0U;
}

void fd_table_init(fd_table_t *table) {
    if (table == 0) {
        return;
    }

    for (uint32_t i = 0; i < FD_TABLE_MAX; ++i) {
        fd_entry_clear(&table->entries[i]);
    }

    table->entries[0].kind = FD_KIND_CONSOLE_IN;
    table->entries[1].kind = FD_KIND_CONSOLE_OUT;
    table->entries[2].kind = FD_KIND_CONSOLE_OUT;
}

int fd_is_writable(const fd_table_t *table, uint32_t fd) {
    if (table == 0 || fd >= FD_TABLE_MAX) {
        return 0;
    }

    switch (table->entries[fd].kind) {
        case FD_KIND_CONSOLE_OUT:
            return 1;

        case FD_KIND_VFS:
            return table->entries[fd].node != 0 &&
                   table->entries[fd].node->write != 0;

        default:
            return 0;
    }
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
