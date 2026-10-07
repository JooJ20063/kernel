#include <kernel/uaccess.h>
#include <kernel/vmm.h>
#include <stdint.h>

#define UACCESS_PAGE_SIZE 0x1000U
#define UACCESS_PAGE_MASK (~(uintptr_t)(UACCESS_PAGE_SIZE - 1U))
#define UACCESS_PTR_MAX   (~(uintptr_t)0)

static int page_allows_user(uintptr_t addr, int write_access) {
    uint32_t flags;

    if (vmm_get_page_flags(addr, &flags) != 0) {
        return 0;
    }

    if ((flags & VMM_PAGE_USER) == 0U) {
        return 0;
    }

    if (write_access && (flags & VMM_PAGE_RW) == 0U) {
        return 0;
    }

    return 1;
}

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

    if ((uintptr_t)(len - 1U) > UACCESS_PTR_MAX - start) {
        return 0;
    }

    end = start + (uintptr_t)(len - 1U);
    page = start & UACCESS_PAGE_MASK;
    last_page = end & UACCESS_PAGE_MASK;

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
