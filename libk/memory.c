#include <libk/memory.h>

/*
 * Preenche 'count' bytes do destino com 'value' convertido para
 * unsigned char. Retorna o ponteiro original do destino.
 */
void *memset(void *dest, int value, size_t count) {
    unsigned char *dst = (unsigned char *)dest;

    while (count-- > 0) {
        *dst++ = (unsigned char)value;
    }

    return dest;
}

/*
 * Copia 'count' bytes da origem para o destino.
 * As regiões não podem se sobrepor; nesse caso, deve-se usar memmove().
 * Retorna o ponteiro original do destino.
 */
void *memcpy(void *dest, const void *src, size_t count) {
    unsigned char *dst = (unsigned char *)dest;
    const unsigned char *source = (const unsigned char *)src;

    while (count-- > 0) {
        *dst++ = *source++;
    }

    return dest;
}

/*
 * Copia 'count' bytes permitindo sobreposição entre origem e destino.
 * A direção da cópia impede que bytes ainda não lidos sejam sobrescritos.
 * Retorna o ponteiro original do destino.
 */
void *memmove(void *dest, const void *src, size_t count) {
    unsigned char *dst = (unsigned char *)dest;
    const unsigned char *source = (const unsigned char *)src;

    /* Nenhuma cópia é necessária para regiões idênticas ou tamanho zero. */
    if (dst == source || count == 0) {
        return dest;
    }

    if (dst < source) {
        /*
         * Destino anterior à origem: copia do início para o fim,
         * preservando os bytes da origem ainda não consumidos.
         */
        while (count-- > 0) {
            *dst++ = *source++;
        }
    } else {
        /*
         * Destino posterior à origem: copia do fim para o início.
         * Os ponteiros começam uma posição após as respectivas regiões
         * e são decrementados antes de cada acesso.
         */
        dst += count;
        source += count;

        while (count-- > 0) {
            *--dst = *--source;
        }
    }

    return dest;
}

/*
 * Compara as regiões byte a byte, interpretando os valores como
 * unsigned char. A primeira diferença determina o sinal do resultado.
 * Retorna zero se todos os 'count' bytes forem iguais.
 */
int memcmp(const void *lhs, const void *rhs, size_t count) {
    const unsigned char *left = (const unsigned char *)lhs;
    const unsigned char *right = (const unsigned char *)rhs;

    while (count-- > 0) {
        if (*left != *right) {
            return (int)*left - (int)*right;
        }

        left++;
        right++;
    }

    return 0;
}