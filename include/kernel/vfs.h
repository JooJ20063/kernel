#pragma once

#include <stdint.h>

/* Flags de atributos e capacidades do nó do sistema de arquivos */
#define FS_FILE      0x01U  // Nó representa um arquivo regular
#define FS_DIRECTORY 0x02U  // Nó representa um diretório
#define FS_WRITABLE  0x04U  // O nó permite operações de escrita
#define FS_SEEKABLE  0x08U  // O nó suporta reposicionamento do ponteiro de E/S (seek)

/* Tamanho máximo para o nome de um nó no VFS */
#define VFS_NAME_MAX 128U

typedef struct fs_node fs_node_t;

/* Assinaturas de ponteiros de função para operações abstratas do VFS */
typedef uint32_t (*read_type_t)(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
typedef uint32_t (*write_type_t)(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
typedef void (*open_type_t)(fs_node_t *node);
typedef void (*close_type_t)(fs_node_t *node);
typedef fs_node_t *(*readdir_type_t)(fs_node_t *node, uint32_t index);
typedef fs_node_t *(*finddir_type_t)(fs_node_t *node, const char *name);
typedef fs_node_t *(*create_type_t)(fs_node_t *node, const char *name, uint32_t flags);
typedef int (*remove_type_t)(fs_node_t *node, const char *name);

/**
 * Estrutura do Nó do Virtual File System (fs_node_t).
 * Abstrai qualquer arquivo, diretório ou dispositivo (/dev) do sistema,
 * mapeando suas rotinas através de uma vtable de ponteiros de função.
 */
struct fs_node {
    char name[VFS_NAME_MAX]; // Nome do nó/arquivo
    uint32_t flags;          // Atributos do nó (FS_FILE, FS_DIRECTORY, etc.)
    uint32_t size;           // Tamanho em bytes
    read_type_t read;        // Callback de leitura
    write_type_t write;      // Callback de escrita
    open_type_t open;        // Callback de abertura
    close_type_t close;      // Callback de fechamento
    readdir_type_t readdir;  // Callback para iterar sobre entradas de diretório
    finddir_type_t finddir;  // Callback para buscar um elemento específico em um diretório
    create_type_t create;    // Callback para criação de um novo nó
    remove_type_t remove;    // Callback para remoção de um nó
    void *device;            // Ponteiro opcional para dados privados do driver subjacente
};

/* Wrappers diretos para invocação dos callbacks de um fs_node_t */
uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
void open_fs(fs_node_t *node);
void close_fs(fs_node_t *node);
fs_node_t *readdir_fs(fs_node_t *node, uint32_t index);
fs_node_t *finddir_fs(fs_node_t *node, const char *name);
fs_node_t *create_fs(fs_node_t *node, const char *name, uint32_t flags);
int remove_fs(fs_node_t *node, const char *name);

/* Gerenciamento do nó raiz e resolução de caminhos (paths) */
void vfs_set_root(fs_node_t *root);
fs_node_t *vfs_root(void);
fs_node_t *vfs_resolve(const char *path);
fs_node_t *vfs_create(const char *path, uint32_t flags);
int vfs_remove(const char *path);

/* Gestão de pontos de montagem (Mount Points) do VFS */
int vfs_mount(const char *path, fs_node_t *root);
int vfs_unmount(const char *path);
uint32_t vfs_mount_count(void);