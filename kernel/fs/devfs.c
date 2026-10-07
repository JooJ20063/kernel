#include <kernel/devfs.h>

static fs_node_t devfs_root_node;
static fs_node_t devfs_null_node;
static fs_node_t devfs_zero_node;

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

static void devfs_noop(fs_node_t *node) {
    (void)node;
}

static uint32_t devfs_null_read(
    fs_node_t *node,
    uint32_t offset,
    uint32_t size,
    uint8_t *buffer
) {
    (void)node;
    (void)offset;
    (void)size;
    (void)buffer;
    return 0U;
}

static uint32_t devfs_discard_write(
    fs_node_t *node,
    uint32_t offset,
    uint32_t size,
    const uint8_t *buffer
) {
    (void)node;
    (void)offset;

    if (buffer == 0 && size != 0U) {
        return 0U;
    }

    return size;
}

static uint32_t devfs_zero_read(
    fs_node_t *node,
    uint32_t offset,
    uint32_t size,
    uint8_t *buffer
) {
    (void)node;
    (void)offset;

    if (buffer == 0 && size != 0U) {
        return 0U;
    }

    for (uint32_t i = 0U; i < size; ++i) {
        buffer[i] = 0U;
    }

    return size;
}

static fs_node_t *devfs_readdir(fs_node_t *node, uint32_t index) {
    if (node != &devfs_root_node) {
        return 0;
    }

    if (index == 0U) {
        return &devfs_null_node;
    }

    if (index == 1U) {
        return &devfs_zero_node;
    }

    return 0;
}

static fs_node_t *devfs_finddir(fs_node_t *node, const char *name) {
    if (node != &devfs_root_node || name == 0) {
        return 0;
    }

    if (str_eq(name, "null")) {
        return &devfs_null_node;
    }

    if (str_eq(name, "zero")) {
        return &devfs_zero_node;
    }

    return 0;
}

static void devfs_clear_node(fs_node_t *node) {
    for (uint32_t i = 0U; i < (uint32_t)sizeof(*node); ++i) {
        ((uint8_t *)node)[i] = 0U;
    }
}

static void devfs_init_file(
    fs_node_t *node,
    const char *name,
    read_type_t read
) {
    devfs_clear_node(node);
    str_copy_limit(node->name, name, sizeof(node->name));
    node->flags = FS_FILE | FS_WRITABLE;
    node->read = read;
    node->write = devfs_discard_write;
    node->open = devfs_noop;
    node->close = devfs_noop;
}

int devfs_init(void) {
    devfs_clear_node(&devfs_root_node);
    str_copy_limit(
        devfs_root_node.name,
        "dev",
        sizeof(devfs_root_node.name)
    );
    devfs_root_node.flags = FS_DIRECTORY;
    devfs_root_node.readdir = devfs_readdir;
    devfs_root_node.finddir = devfs_finddir;
    devfs_root_node.open = devfs_noop;
    devfs_root_node.close = devfs_noop;

    devfs_init_file(
        &devfs_null_node,
        "null",
        devfs_null_read
    );

    devfs_init_file(
        &devfs_zero_node,
        "zero",
        devfs_zero_read
    );

    return vfs_mount("/dev", &devfs_root_node);
}

fs_node_t *devfs_root(void) {
    return &devfs_root_node;
}
