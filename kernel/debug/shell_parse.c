#include "shell_internal.h"

/* Comparação exata de duas strings de texto */
int shell_str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/* Verifica se uma string inicia com um determinado prefixo */
int shell_str_starts(const char *a, const char *prefix) {
    while (*prefix) {
        if (*a++ != *prefix++) {
            return 0;
        }
    }
    return 1;
}

/* Retorna o comprimento de uma string em bytes */
uint32_t shell_str_len(const char *s) {
    uint32_t n = 0;
    while (s[n] != 0) {
        n++;
    }
    return n;
}

/* Localiza a primeira ocorrência de uma sub-string dentro de um buffer de texto */
int shell_str_find(const char *s, const char *pat) {
    uint32_t i = 0;

    if (pat[0] == 0) {
        return 0;
    }

    while (s[i] != 0) {
        uint32_t j = 0;
        while (pat[j] != 0 && s[i + j] != 0 && s[i + j] == pat[j]) {
            j++;
        }

        if (pat[j] == 0) {
            return (int)i;
        }

        i++;
    }

    return -1;
}

/* Copia com segurança um intervalo delimitado de caracteres de uma string para um buffer de destino */
void shell_str_copy_range(char *dst, uint32_t dst_cap, const char *src, uint32_t start, uint32_t end) {
    uint32_t i = 0;

    if (dst_cap == 0) {
        return;
    }

    while ((start + i) < end && src[start + i] != 0 && i + 1U < dst_cap) {
        dst[i] = src[start + i];
        i++;
    }

    dst[i] = 0;
}

/* Converte uma string de dígitos decimais para um inteiro de 32 bits sem sinal */
uint32_t shell_parse_u32(const char *s, int *ok) {
    uint32_t v = 0;
    *ok = 0;

    if (*s == 0) {
        return 0;
    }

    while (*s) {
        if (*s < '0' || *s > '9') {
            return 0;
        }

        v = (v * 10U) + (uint32_t)(*s - '0');
        s++;
    }

    *ok = 1;
    return v;
}

/* Identifica delimitadores de espaço em branco (espaço ou tabulação) */
static int is_space(char c) {
    return (c == ' ' || c == '\t');
}

/* Avanca o ponteiro de caractere pulando caracteres de espaço em branco */
const char *shell_skip_spaces(const char *s) {
    while (*s && is_space(*s)) {
        s++;
    }
    return s;
}

/* Converte uma representação hexadecimal em string (ex: "0x1000") para uint32_t */
uint32_t shell_parse_hex_u32(const char *s, int *ok) {
    uint32_t v = 0;
    *ok = 0;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    if (*s == 0) {
        return 0;
    }

    while (*s) {
        char c = *s;
        uint32_t d;

        if (c >= '0' && c <= '9') {
            d = (uint32_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = 10U + (uint32_t)(c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            d = 10U + (uint32_t)(c - 'A');
        } else {
            return 0;
        }

        v = (v << 4) | d;
        s++;
    }

    *ok = 1;
    return v;
}

