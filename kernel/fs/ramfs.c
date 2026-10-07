#include <kernel/ramfs.h>
#include <kernel/kmalloc.h>

#define TAR_BLOCK_SIZE 512U
#define RAMFS_NEW_FILE_CAPACITY 256U

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};

struct ramfs_entry {
    fs_node_t node;
    uintptr_t data_ptr;
    uint32_t capacity;
    uint8_t writable;
    struct ramfs_entry *parent;
    struct ramfs_entry *first_child;
    struct ramfs_entry *last_child;
    struct ramfs_entry *next_sibling;
};

static struct ramfs_entry ramfs_root_entry;

static uint32_t ramfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static uint32_t ramfs_write(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
static fs_node_t *ramfs_readdir(fs_node_t *node, uint32_t index);
static fs_node_t *ramfs_finddir(fs_node_t *node, const char *name);
static fs_node_t *ramfs_create(fs_node_t *node, const char *name, uint32_t flags);

static void mem_zero(uint8_t *dst, uint32_t size) {
    for (uint32_t i = 0U; i < size; ++i) {
        dst[i] = 0U;
    }
}

static void mem_copy(uint8_t *dst, const uint8_t *src, uint32_t size) {
    for (uint32_t i = 0U; i < size; ++i) {
        dst[i] = src[i];
    }
}

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

static uint32_t str_len(const char *s) {
    uint32_t n = 0U;
    while (s[n] != 0) {
        n++;
    }
    return n;
}

static void str_copy_limit(char *dst, const char *src, uint32_t limit) {
    uint32_t i = 0U;

    if (limit == 0U) {
        return;
    }

    while (src[i] != 0 && i + 1U < limit) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static void tar_name_copy(char dst[VFS_NAME_MAX], const char *src, uint32_t src_limit) {
    uint32_t i = 0U;

    while (i < src_limit && src[i] != 0 && i + 1U < VFS_NAME_MAX) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static uint32_t oct_to_u32(const char *oct, uint32_t len) {
    uint32_t out = 0U;

    for (uint32_t i = 0U; i < len; ++i) {
        char c = oct[i];

        if (c == 0 || c == ' ') {
            break;
        }

        if (c < '0' || c > '7') {
            break;
        }

        out = (out << 3) + (uint32_t)(c - '0');
    }

    return out;
}

static uint8_t tar_is_zero_block(const uint8_t *blk) {
    for (uint32_t i = 0U; i < TAR_BLOCK_SIZE; ++i) {
        if (blk[i] != 0U) {
            return 0U;
        }
    }
    return 1U;
}

static int path_next_component(const char **cursor, char component[VFS_NAME_MAX]) {
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

static struct ramfs_entry *ramfs_entry_from_node(fs_node_t *node) {
    if (node == 0 || node->device == 0) {
        return 0;
    }
    return (struct ramfs_entry *)node->device;
}

static struct ramfs_entry *ramfs_find_child_entry(struct ramfs_entry *parent, const char *name) {
    struct ramfs_entry *cur;

    if (parent == 0 || name == 0) {
        return 0;
    }

    cur = parent->first_child;
    while (cur != 0) {
        if (str_eq(cur->node.name, name)) {
            return cur;
        }
        cur = cur->next_sibling;
    }

    return 0;
}

static void ramfs_link_child(struct ramfs_entry *parent, struct ramfs_entry *child) {
    child->parent = parent;

    if (parent->last_child == 0) {
        parent->first_child = child;
        parent->last_child = child;
        return;
    }

    parent->last_child->next_sibling = child;
    parent->last_child = child;
}

static void ramfs_noop(fs_node_t *node) {
    (void)node;
}

static struct ramfs_entry *ramfs_create_entry(
    struct ramfs_entry *parent,
    const char *name,
    uint32_t flags,
    uintptr_t data_ptr,
    uint32_t size,
    uint32_t capacity,
    uint8_t writable
) {
    struct ramfs_entry *entry;
    struct ramfs_entry *existing;
    uint32_t type = flags & (FS_FILE | FS_DIRECTORY);

    if (parent == 0 || name == 0 || *name == 0 ||
        str_len(name) >= VFS_NAME_MAX ||
        type == 0U || type == (FS_FILE | FS_DIRECTORY)) {
        return 0;
    }

    for (uint32_t i = 0U; name[i] != 0; ++i) {
        if (name[i] == '/') {
            return 0;
        }
    }

    existing = ramfs_find_child_entry(parent, name);
    if (existing != 0) {
        if ((existing->node.flags & type) == 0U) {
            return 0;
        }
        return existing;
    }

    entry = (struct ramfs_entry *)kmalloc((uint32_t)sizeof(struct ramfs_entry));
    if (entry == 0) {
        return 0;
    }

    mem_zero((uint8_t *)entry, (uint32_t)sizeof(struct ramfs_entry));
    str_copy_limit(entry->node.name, name, sizeof(entry->node.name));

    entry->node.size = size;
    entry->node.open = ramfs_noop;
    entry->node.close = ramfs_noop;
    entry->node.device = entry;

    if (type == FS_DIRECTORY) {
        entry->node.flags = FS_DIRECTORY;
        entry->node.readdir = ramfs_readdir;
        entry->node.finddir = ramfs_finddir;
        entry->node.create = ramfs_create;
    } else {
        entry->node.flags = FS_FILE | (writable ? FS_WRITABLE : 0U);
        entry->node.read = ramfs_read;
        entry->node.write = ramfs_write;
        entry->data_ptr = data_ptr;
        entry->capacity = capacity;
        entry->writable = writable;
    }

    ramfs_link_child(parent, entry);
    return entry;
}

static uint32_t ramfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    struct ramfs_entry *entry;

    if (node == 0 || buffer == 0 || node->device == 0) {
        return 0U;
    }

    if ((node->flags & FS_FILE) == 0U || offset >= node->size) {
        return 0U;
    }

    if (offset + size < offset) {
        return 0U;
    }

    if (offset + size > node->size) {
        size = node->size - offset;
    }

    entry = (struct ramfs_entry *)node->device;

    for (uint32_t i = 0U; i < size; ++i) {
        buffer[i] = *((uint8_t *)(entry->data_ptr + offset + i));
    }

    return size;
}

static uint32_t ramfs_write(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    struct ramfs_entry *entry;
    uint32_t need;

    if (node == 0 || buffer == 0 || node->device == 0) {
        return 0U;
    }

    if ((node->flags & FS_FILE) == 0U) {
        return 0U;
    }

    entry = (struct ramfs_entry *)node->device;
    if (entry->writable == 0U) {
        return 0U;
    }

    if (size > 0xFFFFFFFFU - offset) {
        return 0U;
    }

    need = offset + size;

    if (need > entry->capacity) {
        uint32_t new_capacity = entry->capacity;
        uint8_t *new_data;

        if (new_capacity == 0U) {
            new_capacity = RAMFS_NEW_FILE_CAPACITY;
        }

        while (new_capacity < need) {
            if (new_capacity > 0x7FFFFFFFU) {
                return 0U;
            }
            new_capacity <<= 1;
        }

        new_data = (uint8_t *)kmalloc(new_capacity);
        if (new_data == 0) {
            return 0U;
        }

        mem_zero(new_data, new_capacity);

        if (node->size > 0U) {
            mem_copy(new_data, (const uint8_t *)(uintptr_t)entry->data_ptr, node->size);
        }

        if (entry->data_ptr != 0U) {
            kfree((void *)(uintptr_t)entry->data_ptr);
        }

        entry->data_ptr = (uintptr_t)new_data;
        entry->capacity = new_capacity;
    }

    mem_copy((uint8_t *)(uintptr_t)(entry->data_ptr + offset), buffer, size);

    if (need > node->size) {
        node->size = need;
    }

    return size;
}

static fs_node_t *ramfs_readdir(fs_node_t *node, uint32_t index) {
    struct ramfs_entry *entry;
    struct ramfs_entry *cur;
    uint32_t current = 0U;

    if (node == 0 || (node->flags & FS_DIRECTORY) == 0U) {
        return 0;
    }

    entry = ramfs_entry_from_node(node);
    if (entry == 0) {
        return 0;
    }

    cur = entry->first_child;
    while (cur != 0) {
        if (current == index) {
            return &cur->node;
        }
        current++;
        cur = cur->next_sibling;
    }

    return 0;
}

static fs_node_t *ramfs_finddir(fs_node_t *node, const char *name) {
    struct ramfs_entry *entry;
    struct ramfs_entry *child;

    if (node == 0 || name == 0 || (node->flags & FS_DIRECTORY) == 0U) {
        return 0;
    }

    entry = ramfs_entry_from_node(node);
    if (entry == 0) {
        return 0;
    }

    child = ramfs_find_child_entry(entry, name);
    return child != 0 ? &child->node : 0;
}

static fs_node_t *ramfs_create(fs_node_t *node, const char *name, uint32_t flags) {
    struct ramfs_entry *parent;
    struct ramfs_entry *entry;
    uint32_t type = flags & (FS_FILE | FS_DIRECTORY);

    if (node == 0 || name == 0 || (node->flags & FS_DIRECTORY) == 0U) {
        return 0;
    }

    parent = ramfs_entry_from_node(node);
    if (parent == 0) {
        return 0;
    }

    if (type == FS_DIRECTORY) {
        entry = ramfs_create_entry(parent, name, FS_DIRECTORY, 0U, 0U, 0U, 0U);
    } else if (type == FS_FILE) {
        uint8_t *data = (uint8_t *)kmalloc(RAMFS_NEW_FILE_CAPACITY);

        if (data == 0) {
            return 0;
        }

        mem_zero(data, RAMFS_NEW_FILE_CAPACITY);

        entry = ramfs_create_entry(
            parent,
            name,
            FS_FILE | FS_WRITABLE,
            (uintptr_t)data,
            0U,
            RAMFS_NEW_FILE_CAPACITY,
            1U
        );

        if (entry == 0) {
            kfree(data);
        }
    } else {
        return 0;
    }

    return entry != 0 ? &entry->node : 0;
}

static int ramfs_add_tar_path(const char *path, uint8_t directory, uintptr_t data_ptr, uint32_t size) {
    struct ramfs_entry *parent = &ramfs_root_entry;
    const char *cursor = path;
    char component[VFS_NAME_MAX];

    for (;;) {
        const char *after;
        struct ramfs_entry *child;
        int result = path_next_component(&cursor, component);

        if (result < 0) {
            return -1;
        }
        if (result == 0) {
            return 0;
        }
        if (str_eq(component, ".")) {
            continue;
        }
        if (str_eq(component, "..")) {
            return -1;
        }

        after = cursor;
        while (*after == '/') {
            after++;
        }

        child = ramfs_find_child_entry(parent, component);

        if (*after != 0) {
            if (child == 0) {
                child = ramfs_create_entry(parent, component, FS_DIRECTORY, 0U, 0U, 0U, 0U);
            }

            if (child == 0 || (child->node.flags & FS_DIRECTORY) == 0U) {
                return -1;
            }

            parent = child;
            continue;
        }

        if (child != 0) {
            return 0;
        }

        child = ramfs_create_entry(
            parent,
            component,
            directory ? FS_DIRECTORY : FS_FILE,
            directory ? 0U : data_ptr,
            directory ? 0U : size,
            directory ? 0U : size,
            0U
        );

        return child != 0 ? 0 : -1;
    }
}

static void ramfs_create_standard_root(void) {
    static const char *const dirs[] = {
        "/bin",
        "/dev",
        "/proc",
        "/etc",
        "/lib",
        "/tmp"
    };

    for (uint32_t i = 0U; i < (uint32_t)(sizeof(dirs) / sizeof(dirs[0])); ++i) {
        (void)vfs_create(dirs[i], FS_DIRECTORY);
    }
}

void init_ramfs(uintptr_t start, uintptr_t end) {
    uintptr_t p = start;

    mem_zero((uint8_t *)&ramfs_root_entry, (uint32_t)sizeof(ramfs_root_entry));
    str_copy_limit(ramfs_root_entry.node.name, "/", sizeof(ramfs_root_entry.node.name));

    ramfs_root_entry.node.flags = FS_DIRECTORY;
    ramfs_root_entry.node.readdir = ramfs_readdir;
    ramfs_root_entry.node.finddir = ramfs_finddir;
    ramfs_root_entry.node.create = ramfs_create;
    ramfs_root_entry.node.open = ramfs_noop;
    ramfs_root_entry.node.close = ramfs_noop;
    ramfs_root_entry.node.device = &ramfs_root_entry;

    vfs_set_root(&ramfs_root_entry.node);
    ramfs_create_standard_root();

    while (p + TAR_BLOCK_SIZE <= end) {
        struct tar_header *hdr = (struct tar_header *)(uintptr_t)p;
        uint32_t file_size;
        uint32_t blocks;
        char path[VFS_NAME_MAX];

        if (tar_is_zero_block((const uint8_t *)hdr)) {
            break;
        }

        file_size = oct_to_u32(hdr->size, sizeof(hdr->size));
        blocks = (file_size + TAR_BLOCK_SIZE - 1U) / TAR_BLOCK_SIZE;
        tar_name_copy(path, hdr->name, sizeof(hdr->name));

        if (path[0] != 0) {
            (void)ramfs_add_tar_path(
                path,
                (uint8_t)(hdr->typeflag == '5'),
                p + TAR_BLOCK_SIZE,
                file_size
            );
        }

        p += TAR_BLOCK_SIZE + ((uintptr_t)blocks * TAR_BLOCK_SIZE);
    }
}

fs_node_t *ramfs_root(void) {
    return &ramfs_root_entry.node;
}

fs_node_t *ramfs_find(const char *path) {
    return vfs_resolve(path);
}

fs_node_t *ramfs_touch(const char *path) {
    return vfs_create(path, FS_FILE | FS_WRITABLE);
}

fs_node_t *ramfs_mkdir(const char *path) {
    return vfs_create(path, FS_DIRECTORY);
}
