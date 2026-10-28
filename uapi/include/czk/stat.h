#ifndef CZK_UAPI_STAT_H
#define CZK_UAPI_STAT_H

#include <stdint.h>

/*
 * Metadados compartilhados entre kernel e userspace.
 * Preenchidos pela syscall fstat() para um descritor aberto.
 *
 * Layout da ABI x86 atual:
 *   offset 0: st_size  (4 bytes)
 *   offset 4: st_flags (4 bytes)
 *   tamanho total: 8 bytes
 *
 * A ordem, os tipos e os tamanhos dos campos fazem parte da ABI.
 */
typedef struct czk_stat {
    /* Tamanho em bytes informado pelo nó correspondente no VFS. */
    uint32_t st_size;

    /* Flags que identificam o tipo e os atributos expostos pelo VFS. */
    uint32_t st_flags;
} czk_stat_t;

#endif /* CZK_UAPI_STAT_H */