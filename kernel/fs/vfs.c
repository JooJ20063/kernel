#include <kernel/vfs.h>

static fs_node_t *vfs_root_node;

static int str_eq(const char *a, const char *b) {
    while (*a != 0 && *b != 0) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static int vfs_next_component(
    const char **cursor,
    char component[VFS_NAME_MAX]
) {
    const char *p;
    uint32_t length = 0U;

    if (cursor == 0 || *cursor == 0 || component == 0) {
        return -1;
    }

    p = *cursor;
    while (*p == '/') {
        p++;
    }

    if (*p == 0) {
        *cursor = p;
        component[0] = 0;
        return 0;
    }

    while (*p != 0 && *p != '/') {
        if (length + 1U >= VFS_NAME_MAX) {
            return -1;
        }
        component[length++] = *p++;
    }

    component[length] = 0;
    *cursor = p;
    return 1;
}

uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (node == 0 || node->read == 0) {
        return 0;
    }
    return node->read(node, offset, size, buffer);
}

uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    if (node == 0 || node->write == 0) {
        return 0;
    }
    return node->write(node, offset, size, buffer);
}

void open_fs(fs_node_t *node) {
    if (node != 0 && node->open != 0) {
        node->open(node);
    }
}

void close_fs(fs_node_t *node) {
    if (node != 0 && node->close != 0) {
        node->close(node);
    }
}

fs_node_t *readdir_fs(fs_node_t *node, uint32_t index) {
    if (node == 0 || node->readdir == 0) {
        return 0;
    }
    return node->readdir(node, index);
}

fs_node_t *finddir_fs(fs_node_t *node, const char *name) {
    if (node == 0 || name == 0 || node->finddir == 0) {
        return 0;
    }
    return node->finddir(node, name);
}

fs_node_t *create_fs(fs_node_t *node, const char *name, uint32_t flags) {
    if (node == 0 || name == 0 || node->create == 0) {
        return 0;
    }
    return node->create(node, name, flags);
}

void vfs_set_root(fs_node_t *root) {
    vfs_root_node = root;
}

fs_node_t *vfs_root(void) {
    return vfs_root_node;
}

fs_node_t *vfs_resolve(const char *path) {
    fs_node_t *node = vfs_root_node;
    const char *cursor = path;
    char component[VFS_NAME_MAX];

    if (node == 0 || path == 0 || *path == 0) {
        return 0;
    }

    for (;;) {
        int result = vfs_next_component(&cursor, component);

        if (result < 0) {
            return 0;
        }
        if (result == 0) {
            return node;
        }
        if (str_eq(component, ".")) {
            continue;
        }
        if (str_eq(component, "..")) {
            return 0;
        }
        if ((node->flags & FS_DIRECTORY) == 0U) {
            return 0;
        }

        node = finddir_fs(node, component);
        if (node == 0) {
            return 0;
        }
    }
}

static int vfs_resolve_parent(
    const char *path,
    fs_node_t **parent_out,
    char name_out[VFS_NAME_MAX]
) {
    fs_node_t *node = vfs_root_node;
    const char *cursor = path;
    char component[VFS_NAME_MAX];

    if (node == 0 || path == 0 || *path == 0 ||
        parent_out == 0 || name_out == 0) {
        return -1;
    }

    for (;;) {
        const char *after;
        int result = vfs_next_component(&cursor, component);

        if (result <= 0) {
            return -1;
        }

        after = cursor;
        while (*after == '/') {
            after++;
        }

        if (*after == 0) {
            uint32_t i = 0U;

            if (str_eq(component, ".") || str_eq(component, "..")) {
                return -1;
            }

            while (component[i] != 0) {
                name_out[i] = component[i];
                i++;
            }
            name_out[i] = 0;
            *parent_out = node;
            return 0;
        }

        if (str_eq(component, ".")) {
            continue;
        }
        if (str_eq(component, "..") ||
            (node->flags & FS_DIRECTORY) == 0U) {
            return -1;
        }

        node = finddir_fs(node, component);
        if (node == 0 || (node->flags & FS_DIRECTORY) == 0U) {
            return -1;
        }
    }
}

fs_node_t *vfs_create(const char *path, uint32_t flags) {
    fs_node_t *existing;
    fs_node_t *parent;
    char name[VFS_NAME_MAX];
    uint32_t type = flags & (FS_FILE | FS_DIRECTORY);

    if (type == 0U || type == (FS_FILE | FS_DIRECTORY)) {
        return 0;
    }

    existing = vfs_resolve(path);
    if (existing != 0) {
        if ((existing->flags & type) == 0U) {
            return 0;
        }
        return existing;
    }

    if (vfs_resolve_parent(path, &parent, name) != 0) {
        return 0;
    }

    if ((parent->flags & FS_DIRECTORY) == 0U) {
        return 0;
    }

    return create_fs(parent, name, flags);
}
