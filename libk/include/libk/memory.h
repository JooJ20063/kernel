#ifndef LIBK_MEMORY_H
#define LIBK_MEMORY_H

#include <stddef.h>

/*
 * Preenche os primeiros 'count' bytes de 'dest' com o valor de 'value'
 * convertido para unsigned char.
 * Retorna o ponteiro original 'dest'.
 */
void *memset(void *dest, int value, size_t count);

/*
 * Copia 'count' bytes de 'src' para 'dest'.
 * As regiões de origem e destino não podem se sobrepor.
 * Retorna o ponteiro original 'dest'.
 */
void *memcpy(void *dest, const void *src, size_t count);

/*
 * Copia 'count' bytes de 'src' para 'dest', permitindo sobreposição.
 * O resultado equivale a copiar por meio de um buffer temporário.
 * Retorna o ponteiro original 'dest'.
 */
void *memmove(void *dest, const void *src, size_t count);

/*
 * Compara os primeiros 'count' bytes de 'lhs' e 'rhs', interpretados
 * como unsigned char.
 * Retorna zero se forem iguais, um valor negativo se o primeiro byte
 * diferente de 'lhs' for menor, ou positivo se for maior.
 */
int memcmp(const void *lhs, const void *rhs, size_t count);

#endif /* LIBK_MEMORY_H */