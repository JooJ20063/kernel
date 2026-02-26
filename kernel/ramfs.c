#include <kernel/ramfs.h>
#include <kernel/kmalloc.h>

#define TAR_BLOCK_SIZE 512U

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
    struct ramfs_entry *next;
};

static fs_node_t ramfs_root_node;
static struct ramfs_entry *ramfs_head;

static void mem_zero(uint8_t *dst, uint32_t size) {
    for (uint32_t i = 0; i < size; ++i) {
        dst[i] = 0;
    }
}

static void str_copy_limit(char *dst, const char *src, uint32_t limit) {
    uint32_t i = 0;

    if (limit == 0) {
        return;
    }

    while (src[i] != 0 && i + 1U < limit) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}

static uint32_t oct_to_u32(const char *oct, uint32_t len) {
    uint32_t out = 0;

    for (uint32_t i = 0; i < len; ++i) {
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
    for (uint32_t i = 0; i < TAR_BLOCK_SIZE; ++i) {
        if (blk[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static uint32_t ramfs_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    struct ramfs_entry *entry;

    if (node == 0 || buffer == 0 || node->device == 0) {
        return 0;
    }

    if ((node->flags & FS_FILE) == 0) {
        return 0;
    }

    if (offset >= node->size) {
        return 0;
    }

    if (offset + size > node->size) {
        size = node->size - offset;
    }

    entry = (struct ramfs_entry *)node->device;

    for (uint32_t i = 0; i < size; ++i) {
        buffer[i] = *((uint8_t *)(entry->data_ptr + offset + i));
    }

    return size;
}

static fs_node_t *ramfs_readdir(fs_node_t *node, uint32_t index) {
    struct ramfs_entry *cur;
    uint32_t current_index = 0;

    if (node == 0 || (node->flags & FS_DIRECTORY) == 0) {
        return 0;
    }

    cur = ramfs_head;
    while (cur != 0) {
        if (current_index == index) {
            return &cur->node;
        }
        current_index++;
        cur = cur->next;
    }

    return 0;
}

static uint32_t ramfs_write_unsupported(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    (void)node;
    (void)offset;
    (void)size;
    (void)buffer;
    return 0;
}

static void ramfs_noop(fs_node_t *node) {
    (void)node;
}

void init_ramfs(uintptr_t start, uintptr_t end) {
    uintptr_t p = start;
    struct ramfs_entry *tail = 0;

    ramfs_head = 0;

    mem_zero((uint8_t *)&ramfs_root_node, (uint32_t)sizeof(ramfs_root_node));
    str_copy_limit(ramfs_root_node.name, "/", sizeof(ramfs_root_node.name));
    ramfs_root_node.flags = FS_DIRECTORY;
    ramfs_root_node.readdir = ramfs_readdir;
    ramfs_root_node.open = ramfs_noop;
    ramfs_root_node.close = ramfs_noop;

    while (p + TAR_BLOCK_SIZE <= end) {
        struct tar_header *hdr = (struct tar_header *)p;
        uint32_t file_size;
        uint32_t blocks;

        if (tar_is_zero_block((const uint8_t *)hdr)) {
            break;
        }

        file_size = oct_to_u32(hdr->size, sizeof(hdr->size));
        blocks = (file_size + TAR_BLOCK_SIZE - 1U) / TAR_BLOCK_SIZE;

        if (hdr->name[0] != 0 && hdr->typeflag != '5') {
            struct ramfs_entry *entry = (struct ramfs_entry *)kmalloc((uint32_t)sizeof(struct ramfs_entry));
            if (entry == 0) {
                break;
            }

            mem_zero((uint8_t *)entry, (uint32_t)sizeof(struct ramfs_entry));
            str_copy_limit(entry->node.name, hdr->name, sizeof(entry->node.name));
            entry->node.flags = FS_FILE;
            entry->node.size = file_size;
            entry->node.read = ramfs_read;
            entry->node.write = ramfs_write_unsupported;
            entry->node.open = ramfs_noop;
            entry->node.close = ramfs_noop;
            entry->node.device = entry;
            entry->data_ptr = p + TAR_BLOCK_SIZE;

            if (ramfs_head == 0) {
                ramfs_head = entry;
            } else {
                tail->next = entry;
            }
            tail = entry;
        }

        p += TAR_BLOCK_SIZE + ((uintptr_t)blocks * TAR_BLOCK_SIZE);
    }

}

fs_node_t *ramfs_root(void) {
    return &ramfs_root_node;
}
