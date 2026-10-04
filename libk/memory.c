#include <libk/memory.h>

void *memset(void *dest, int value, size_t count) {
    unsigned char *dst = (unsigned char *)dest;

    while (count-- > 0) {
        *dst++ = (unsigned char)value;
    }

    return dest;
}


void *memcpy(void *dest, const void *src, size_t count) {
    unsigned char *dst = (unsigned char *)dest;
    const unsigned char *source = (const unsigned char *)src;

    while (count-- > 0) {
        *dst++ = *source++;
    }

    return dest;
}


void *memmove(void *dest, const void *src, size_t count) {
    unsigned char *dst = (unsigned char *)dest;
    const unsigned char *source = (const unsigned char *)src;

    if (dst == source || count == 0) {
        return dest;
    }

    if (dst < source) {
        while (count-- > 0) {
            *dst++ = *source++;
        }
    } else {
        dst += count;
        source += count;

        while (count-- > 0) {
            *--dst = *--source;
        }
    }

    return dest;
}
