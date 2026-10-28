#ifndef KERNEL_FD_H
#define KERNEL_FD_H

#include <stdint.h>

/* Número máximo de descritores de arquivos abertos por processo */
#define FD_TABLE_MAX 16U

/* Flags de permissão de acesso do descritor */
#define FD_ACCESS_READ  0x01U
#define FD_ACCESS_WRITE 0x02U

typedef struct fs_node fs_node_t;

/* Tipo/Natureza do descritor de arquivo */
typedef enum fd_kind {
    FD_KIND_NONE = 0, // Entrada livre/fechada na tabela
    FD_KIND_VFS       // Descritor associado a um nó do Virtual File System (VFS)
} fd_kind_t;

/**
 * Entrada da tabela de descritores de arquivos (File Descriptor Entry).
 * Armazena o tipo do recurso, ponteiro para o nó VFS, o cursor atual (offset) e as permissões de E/S.
 */
typedef struct fd_entry {
    fd_kind_t kind;   // Status da entrada (livre ou VFS)
    fs_node_t *node;  // Ponteiro para o nó de arquivo/dispositivo subjacente
    uint32_t offset;  // Posição atual de leitura/escrita no arquivo
    uint32_t access;  // Máscara de permissões (FD_ACCESS_READ / FD_ACCESS_WRITE)
} fd_entry_t;

/* Tabela de descritores alocada por processo (mapeia um número inteiro 'fd' para 'fd_entry_t') */
typedef struct fd_table {
    fd_entry_t entries[FD_TABLE_MAX];
} fd_table_t;

/* Inicializa uma tabela de descritores zerando todas as suas entradas */
void fd_table_init(fd_table_t *table);

/* Vincula os descritores padrão stdin (0), stdout (1) e stderr (2) a um dispositivo TTY */
int fd_table_bind_stdio(fd_table_t *table, fs_node_t *tty_node);

/* Fecha todas as entradas abertas da tabela e libera referências aos nós VFS */
void fd_table_close_all(fd_table_t *table);

/* Helpers de checagem de permissões de leitura e escrita para um descritor específico */
int fd_is_readable(const fd_table_t *table, uint32_t fd);
int fd_is_writable(const fd_table_t *table, uint32_t fd);

/* Associa um nó VFS à primeira entrada livre da tabela e retorna o número do 'fd' alocado (ou < 0 em erro) */
int32_t fd_open_vfs(fd_table_t *table, fs_node_t *node, uint32_t access);

/* Fecha o descritor de arquivo e liberta a entrada na tabela */
int32_t fd_close(fd_table_t *table, uint32_t fd);

/* Realiza leitura no arquivo a partir do offset atual do descritor e incrementa o cursor */
int32_t fd_read(fd_table_t *table, uint32_t fd, uint8_t *buffer, uint32_t size);

/* Realiza escrita no arquivo a partir do offset atual do descritor e incrementa o cursor */
int32_t fd_write(fd_table_t *table, uint32_t fd, const uint8_t *buffer, uint32_t size);

/* Altera a posição do cursor (offset) do descritor com base nos parâmetros whence (SEEK_SET, SEEK_CUR, SEEK_END) */
int32_t fd_seek(fd_table_t *table, uint32_t fd, int32_t offset, uint32_t whence, uint32_t *new_offset);

/* Retorna metadados do arquivo apontado pelo descritor (tamanho em bytes e flags do nó) */
int32_t fd_stat(fd_table_t *table, uint32_t fd, uint32_t *size_out, uint32_t *flags_out);

/* Lê entradas de diretório se o descritor apontar para um nó de diretório no VFS */
int32_t fd_readdir(fd_table_t *table, uint32_t fd, fs_node_t **node_out);

#endif