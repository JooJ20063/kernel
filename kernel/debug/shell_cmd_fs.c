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

/* K0.8a — Testes de montagem e de dispositivos. */
void shell_cmd_mounttest(void) {
    fs_node_t *source;
    fs_node_t *target;
    fs_node_t *probe;
    fs_node_t *mounted_root;
    fs_node_t *mounted_probe;
    uint32_t baseline_mounts = vfs_mount_count();
    uint8_t ok = 1U;

    source = vfs_resolve("/tmp");
    if (source == 0) {
        klog_warn("mounttest: /tmp unavailable");
        return;
    }

    target = vfs_create("/mnttest", FS_DIRECTORY);
    if (target == 0) {
        klog_warn("mounttest: failed to create /mnttest");
        return;
    }

    probe = vfs_create("/tmp/__mount_probe", FS_FILE | FS_WRITABLE);
    if (probe == 0) {
        (void)vfs_remove("/mnttest");
        klog_warn("mounttest: failed to create probe");
        return;
    }

    if (vfs_mount("/mnttest", source) != 0) {
        (void)vfs_remove("/tmp/__mount_probe");
        (void)vfs_remove("/mnttest");
        klog_warn("mounttest: mount failed");
        return;
    }

    mounted_root = vfs_resolve("/mnttest");
    mounted_probe = vfs_resolve("/mnttest/__mount_probe");

    if (mounted_root != source ||
        mounted_probe != probe ||
        vfs_mount_count() != baseline_mounts + 1U) {
        ok = 0U;
    }

    if (vfs_unmount("/mnttest") != 0) {
        ok = 0U;
    }

    if (vfs_resolve("/mnttest") != target ||
        vfs_resolve("/mnttest/__mount_probe") != 0 ||
        vfs_mount_count() != baseline_mounts) {
        ok = 0U;
    }

    if (vfs_remove("/tmp/__mount_probe") != 0 ||
        vfs_remove("/mnttest") != 0) {
        ok = 0U;
    }

    if (vfs_resolve("/tmp/__mount_probe") != 0 ||
        vfs_resolve("/mnttest") != 0) {
        ok = 0U;
    }

    vga_puts("mounttest: result=");
    vga_puts(ok ? "MOUNT OK" : "FAILED");
    vga_puts("\n");
}

void shell_cmd_devtest(void) {
    fs_node_t *null_node = vfs_resolve("/dev/null");
    fs_node_t *zero_node = vfs_resolve("/dev/zero");
    uint8_t zero_buf[32];
    static const uint8_t payload[] = "cruzeiro";
    uint8_t ok = 1U;

    if (null_node == 0 || zero_node == 0) {
        ok = 0U;
    }

    if (ok && read_fs(null_node, 0U, sizeof(zero_buf), zero_buf) != 0U) {
        ok = 0U;
    }

    if (ok && write_fs(null_node, 0U, (uint32_t)(sizeof(payload) - 1U), payload) != (uint32_t)(sizeof(payload) - 1U)) {
        ok = 0U;
    }

    if (ok) {
        for (uint32_t i = 0U; i < sizeof(zero_buf); ++i) {
            zero_buf[i] = 0xA5U;
        }

        if (read_fs(zero_node, 0U, sizeof(zero_buf), zero_buf) != sizeof(zero_buf)) {
            ok = 0U;
        }
    }

    if (ok) {
        for (uint32_t i = 0U; i < sizeof(zero_buf); ++i) {
            if (zero_buf[i] != 0U) {
                ok = 0U;
                break;
            }
        }
    }

    vga_puts("devtest: result=");
    vga_puts(ok ? "DEVFS OK" : "FAILED");
    vga_puts("\n");
}
