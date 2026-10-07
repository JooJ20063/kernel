#ifndef CZK_UAPI_DIRENT_H
#define CZK_UAPI_DIRENT_H

#include <stdint.h>

#define CZK_DIRENT_NAME_MAX 128U

typedef struct czk_dirent {
    char d_name[CZK_DIRENT_NAME_MAX];
    uint32_t d_flags;
    uint32_t d_size;
} czk_dirent_t;

#endif
