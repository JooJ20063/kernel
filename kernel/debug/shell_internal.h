#ifndef CZK_SHELL_INTERNAL_H
#define CZK_SHELL_INTERNAL_H

#include <stdint.h>

/* API privada do monitor Ring 0; nao pertence a UAPI. */
int shell_str_eq(const char *a, const char *b);
int shell_str_starts(const char *a, const char *prefix);
int shell_str_find(const char *s, const char *pat);

void shell_str_copy_range(char *dst, uint32_t dst_cap, const char *src,uint32_t start, uint32_t end);

uint32_t shell_str_len(const char *s);
uint32_t shell_parse_u32(const char *s, int *ok);
uint32_t shell_parse_hex_u32(const char *s, int *ok);

const char *shell_skip_spaces(const char *s);

#endif
