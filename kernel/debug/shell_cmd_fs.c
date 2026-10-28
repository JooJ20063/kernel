#include "shell_internal.h"
#include <kernel/vfs.h>
#include <kernel/vga.h>
#include <kernel/klog.h>

void shell_cmd_ls(const char *path) {
    const char *target = (path == 0 || *path == 0) ? "/" : path;
    fs_node_t *directory = vfs_resolve(target);
    uint32_t i = 0U;

    if (directory == 0) {
        klog_warn("diretorio nao encontrado");
        return;
    }

    if ((directory->flags & FS_DIRECTORY) == 0U) {
        klog_warn("ls: caminho nao e diretorio");
        return;
    }

    vga_puts("entries ");
    vga_puts(target);
    vga_puts(":\n");

    for (;;) {
        fs_node_t *entry = readdir_fs(directory, i);

        if (entry == 0) {
            break;
        }

        vga_puts(" - ");
        vga_puts(entry->name);

        if ((entry->flags & FS_DIRECTORY) != 0U) {
            vga_puts("/");
        } else {
            vga_puts(" (");
            vga_putdec(entry->size);
            vga_puts(" bytes)");
        }

        vga_puts("\n");
        i++;
    }

    if (i == 0U) {
        vga_puts("(vazio)\n");
    }
}

void shell_cmd_cat(const char *name) {
    fs_node_t *entry;

    if (name == 0 || *name == 0) {
        klog_warn("usage: cat <arquivo>");
        return;
    }

    entry = vfs_resolve(name);
    if (entry == 0) {
        klog_warn("arquivo nao encontrado");
        return;
    }

    if ((entry->flags & FS_FILE) == 0U) {
        klog_warn("cat: caminho nao e arquivo");
        return;
    }

    {
        uint8_t buf[64];
        uint32_t off = 0U;

        while (off < entry->size) {
            uint32_t n = read_fs(entry, off, sizeof(buf), buf);

            if (n == 0U) {
                break;
            }

            for (uint32_t j = 0U; j < n; ++j) {
                vga_putc((char)buf[j]);
            }

            off += n;
        }
    }

    vga_puts("\n");
}

void shell_write_text_file(const char *name, const char *text) {
    fs_node_t *entry;
    uint32_t text_length;
    uint32_t n;

    if (name == 0 || *name == 0) {
        klog_warn("arquivo invalido");
        return;
    }

    entry = vfs_create(name, FS_FILE | FS_WRITABLE);
    if (entry == 0) {
        klog_warn("falha ao criar/abrir arquivo");
        return;
    }

    text_length = shell_str_len(text);
    n = write_fs(entry, 0U, text_length, (const uint8_t *)text);

    if (n != text_length) {
        klog_warn("arquivo somente leitura ou sem memoria");
        return;
    }

    vga_puts("ok: ");
    vga_puts(name);
    vga_puts(" <= ");
    vga_putdec(n);
    vga_puts(" bytes\n");
}

void shell_cmd_touch(const char *name) {
    fs_node_t *entry;

    if (name == 0 || *name == 0) {
        klog_warn("usage: touch <arquivo>");
        return;
    }

    entry = vfs_create(name, FS_FILE | FS_WRITABLE);
    if (entry == 0) {
        klog_warn("touch falhou");
        return;
    }

    vga_puts("touch: ");
    vga_puts(name);
    vga_puts("\n");
}

void shell_cmd_mkdir(const char *path) {
    fs_node_t *entry;

    if (path == 0 || *path == 0) {
        klog_warn("usage: mkdir <diretorio>");
        return;
    }

    entry = vfs_create(path, FS_DIRECTORY);
    if (entry == 0 || (entry->flags & FS_DIRECTORY) == 0U) {
        klog_warn("mkdir falhou");
        return;
    }

    vga_puts("mkdir: ");
    vga_puts(path);
    vga_puts("\n");
}
