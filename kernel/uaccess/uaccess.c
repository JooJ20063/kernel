#include <kernel/uaccess.h>
#include <kernel/vmm.h>
#include <stdint.h>

/* Tamanho de página padrão x86 (4KB) e máscaras para alinhamento e limites de 32 bits */
#define UACCESS_PAGE_SIZE 0x1000U
#define UACCESS_PAGE_MASK (~(uintptr_t)(UACCESS_PAGE_SIZE - 1U))
#define UACCESS_PTR_MAX   (~(uintptr_t)0)

/**
 * Verifica se uma determinada página física/virtual pertence ao Ring 3 (User Space)
 * e atende aos requisitos de leitura ou escrita consultando o VMM.
 */
static int page_allows_user(uintptr_t addr, int write_access) {
    uint32_t flags;

    if (vmm_get_page_flags(addr, &flags) != 0) {
        return 0;
    }

    /* A página precisa obrigatoriamente ter a flag VMM_PAGE_USER ativa */
    if ((flags & VMM_PAGE_USER) == 0U) {
        return 0;
    }

    /* Se for operação de escrita, exige a flag VMM_PAGE_RW */
    if (write_access && (flags & VMM_PAGE_RW) == 0U) {
        return 0;
    }

    return 1;
}

/**
 * Valida se um buffer de espaço de usuário (user_ptr + len) está completamente
 * contido em páginas de usuário válidas e acessíveis. Checa por estouros de 32 bits (overflow).
 */
int user_ptr_valid(const void *user_ptr, size_t len, int write_access) {
    uintptr_t start;
    uintptr_t end;
    uintptr_t page;
    uintptr_t last_page;

    if (len == 0U) {
        return 1;
    }

    if (user_ptr == 0) {
        return 0;
    }

    start = (uintptr_t)user_ptr;

    /* Previne estouro de inteiro de 32 bits no cálculo de ponteiro limite */
    if ((uintptr_t)(len - 1U) > UACCESS_PTR_MAX - start) {
        return 0;
    }

    end = start + (uintptr_t)(len - 1U);
    page = start & UACCESS_PAGE_MASK;
    last_page = end & UACCESS_PAGE_MASK;

    /* Valida página por página até cobrir toda a extensão do buffer */
    for (;;) {
        if (!page_allows_user(page, write_access)) {
            return 0;
        }

        if (page == last_page) {
            break;
        }

        if (page > UACCESS_PTR_MAX - UACCESS_PAGE_SIZE) {
            return 0;
        }

        page += UACCESS_PAGE_SIZE;
    }

    return 1;
}

/**
 * Copia dados do espaço de usuário (Ring 3) para um buffer no Kernel (Ring 0)
 * após validar integralmente o intervalo de memória de origem.
 */
int copy_from_user(void *kernel_dst, const void *user_src, size_t len) {
    uint8_t *dst;
    const uint8_t *src;

    if (len == 0U) {
        return 0;
    }

    if (kernel_dst == 0 || !user_ptr_valid(user_src, len, 0)) {
        return -1;
    }

    dst = (uint8_t *)kernel_dst;
    src = (const uint8_t *)user_src;

    for (size_t i = 0; i < len; ++i) {
        dst[i] = src[i];
    }

    return 0;
}

/**
 * Copia dados de um buffer no Kernel (Ring 0) para o espaço de usuário (Ring 3)
 * após validar as permissões de escrita do intervalo de destino.
 */
int copy_to_user(void *user_dst, const void *kernel_src, size_t len) {
    uint8_t *dst;
    const uint8_t *src;

    if (len == 0U) {
        return 0;
    }

    if (kernel_src == 0 || !user_ptr_valid(user_dst, len, 1)) {
        return -1;
    }

    dst = (uint8_t *)user_dst;
    src = (const uint8_t *)kernel_src;

    for (size_t i = 0; i < len; ++i) {
        dst[i] = src[i];
    }

    return 0;
}

/**
 * Copia uma string terminada em NUL do espaço de usuário para um buffer de destino do Kernel.
 * Valida cada caractere lido e previne estouros de buffer (trunca e retorna -2 se exceder dst_size).
 */
int copy_string_from_user(
    char *kernel_dst,
    const char *user_src,
    size_t dst_size
) {
    uintptr_t base;

    if (kernel_dst == 0 || user_src == 0 || dst_size == 0U) {
        return -1;
    }

    base = (uintptr_t)user_src;

    for (size_t i = 0; i < dst_size; ++i) {
        char ch;

        if ((uintptr_t)i > UACCESS_PTR_MAX - base) {
            return -1;
        }

        if (copy_from_user(
                &ch,
                (const void *)(base + (uintptr_t)i),
                1U) != 0) {
            return -1;
        }

        kernel_dst[i] = ch;

        if (ch == 0) {
            return 0;
        }
    }

    /* Retorna erro se a string de usuário for maior que o tamanho do buffer do kernel */
    return -2;
}