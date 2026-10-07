#ifndef CZK_UAPI_DIRENT_H
#define CZK_UAPI_DIRENT_H

#include <stdint.h>
#include <czk/abi.h>
#include <czk/fs.h>

#define CZK_DIRENT_NAME_MAX CZK_NAME_MAX

typedef struct czk_dirent {
    char d_name[CZK_DIRENT_NAME_MAX];
    uint32_t d_flags;
    uint32_t d_size;
} czk_dirent_t;

#endif
