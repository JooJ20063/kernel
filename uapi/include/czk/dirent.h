#ifndef CZK_UAPI_DIRENT_H
#define CZK_UAPI_DIRENT_H

#include <stdint.h>

/* Capacidade do campo de nome em bytes, incluindo o terminador NUL. */
#define CZK_DIRENT_NAME_MAX 128U

/*
 * Entrada de diretório compartilhada entre kernel e userspace.
 * Preenchida pela syscall readdir().
 *
 * Layout da ABI x86 atual:
 *   offset   0: d_name  (128 bytes)
 *   offset 128: d_flags (4 bytes)
 *   offset 132: d_size  (4 bytes)
 *   tamanho total: 136 bytes
 *
 * A ordem, os tipos e os tamanhos dos campos fazem parte da ABI.
 */
typedef struct czk_dirent {
    /* Nome da entrada, terminado em NUL. */
    char d_name[CZK_DIRENT_NAME_MAX];

    /* Flags que identificam o tipo e os atributos expostos pelo VFS. */
    uint32_t d_flags;

    /* Tamanho em bytes informado pelo nó correspondente no VFS. */
    uint32_t d_size;
} czk_dirent_t;

#endif /* CZK_UAPI_DIRENT_H */