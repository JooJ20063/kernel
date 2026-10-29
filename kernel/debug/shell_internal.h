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


/* Comandos internos do monitor Ring 0: filesystem. */
void shell_cmd_ls(const char *path);
void shell_cmd_cat(const char *name);
void shell_cmd_touch(const char *name);
void shell_cmd_mkdir(const char *path);
void shell_write_text_file(const char *name, const char *text);

/* Comandos de diagnostico ELF (privados ao monitor Ring 0). */
void shell_cmd_elftest(const char *path);
void shell_cmd_elfloadtest(const char *path);
void shell_cmd_elfrun(const char *path);


/* Comandos de diagnostico de memoria (internos ao shell Ring 0). */
void shell_cmd_virt(const char *arg);
void shell_cmd_mapped(const char *arg);
void shell_cmd_unmap(const char *arg);
void shell_cmd_kslots(void);
void shell_cmd_kmalloc_slot(const char *arg);
void shell_cmd_kfree_slot(const char *arg);
void shell_cmd_krealloc_slot(const char *arg);
void shell_cmd_tmptest(void);
void shell_memory_init(void);


/* Comandos de tarefas e scheduler (uso interno do monitor Ring 0). */
void shell_cmd_task(void);
void shell_cmd_ps(void);
void shell_cmd_schedtest(void);
void shell_cmd_lastexit(void);
void shell_cmd_waittest(void);


/* Testes especificos da arquitetura x86 (Ring 3). */
#ifndef __x86_64__
void shell_cmd_ring3test(void);
void shell_cmd_ring3fault(void);
void shell_cmd_ring3ud(void);
void shell_cmd_ring3gp(void);
void shell_cmd_ring3as(void);
#endif


/* K0.6a: comandos internos de TTY e stdio. */
void shell_cmd_ttytest(void);
void shell_cmd_stdiotest(void);
void shell_cmd_ttyfocus(const char *arg);


#ifndef __x86_64__
void shell_cmd_ttyblocktest(void);
void shell_cmd_ttyfgtest(void);
void shell_cmd_ttycantest(void);
#endif


/* Diagnosticos destrutivos: uso apenas controlado. */
void shell_cmd_pfault(const char *arg);
void shell_cmd_panic(const char *arg);

/* Testes VFS e DevFS do monitor Ring 0. */
void shell_cmd_mounttest(void);
void shell_cmd_devtest(void);

#endif
