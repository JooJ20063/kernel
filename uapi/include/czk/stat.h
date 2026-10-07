#ifndef CZK_UAPI_STAT_H
#define CZK_UAPI_STAT_H

#include <stdint.h>
#include <czk/fs.h>

typedef struct czk_stat {
    uint32_t st_size;
    uint32_t st_flags;
} czk_stat_t;

#endif
