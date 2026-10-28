#ifndef KERNEL_UACCESS_H
#define KERNEL_UACCESS_H

#include <stddef.h>

/**
 * Valida se um intervalo de endereços apontado por 'user_ptr' reside inteiramente no espaço de usuário (Ring 3)
 * e se as permissões de acesso esperadas (leitura ou escrita via 'write_access') são respeitadas.
 * Previne ataques do tipo "Confused Deputy" e leituras/escritas não autorizadas na memória do Kernel.
 */
int user_ptr_valid(const void *user_ptr, size_t len, int write_access);

/**
 * Copia com segurança 'len' bytes do espaço de endereçamento de usuário ('user_src') para um buffer do Kernel ('kernel_dst').
 * Retorna 0 em caso de sucesso ou um valor diferente de zero caso haja falha de acesso à memória (Page Fault / Violação de Proteção).
 */
int copy_from_user(void *kernel_dst, const void *user_src, size_t len);

/**
 * Copia com segurança 'len' bytes de um buffer do Kernel ('kernel_src') para o espaço de endereçamento do usuário ('user_dst').
 * Retorna 0 em caso de sucesso ou um valor diferente de zero em caso de violação de memória no Ring 3.
 */
int copy_to_user(void *user_dst, const void *kernel_src, size_t len);

/**
 * Copia uma string terminada em nulo do espaço de usuário ('user_src') para um buffer do Kernel ('kernel_dst')
 * de até 'dst_size' bytes, garantindo que a string resultante no kernel permaneça terminada em nulo.
 * Retorna 0 em caso de sucesso ou um valor negativo/não-nulo se houver falha de cópia ou estouro de buffer.
 */
int copy_string_from_user(char *kernel_dst, const char *user_src, size_t dst_size);

#endif