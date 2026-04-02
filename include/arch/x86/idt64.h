#ifndef ARCH_X86_IDT64_H
#define ARCH_X86_IDT64_H

#include <stddef.h>

struct idt_entry {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char ist;
    unsigned char type_attr;
    unsigned short offset_mid;
    unsigned int offset_high;
    unsigned int reserved;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned long base;
} __attribute__((packed));

void idt_init64(void);

#endif
