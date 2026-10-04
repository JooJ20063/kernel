#include <libk/memory.h>

void *memset(void *dest, int value, size_t count) {
    unsigned char *dst = (unsigned char *)dest;

    while (count-- > 0) {
        *dst++ = (unsigned char)value;
    }

    return dest;
}
