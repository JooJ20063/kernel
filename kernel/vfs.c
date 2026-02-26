#include <kernel/vfs.h>

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
