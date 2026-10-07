#ifndef KERNEL_UACCESS_H
#define KERNEL_UACCESS_H

#include <stddef.h>

int user_ptr_valid(const void *user_ptr, size_t len, int write_access);
int copy_from_user(void *kernel_dst, const void *user_src, size_t len);
int copy_to_user(void *user_dst, const void *kernel_src, size_t len);
int copy_string_from_user(char *kernel_dst, const char *user_src, size_t dst_size);

#endif
