#ifndef ARCH_X86_64_IDT_H
#define ARCH_X86_64_IDT_H

<<<<<<< HEAD
=======
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

>>>>>>> 4cc05da (Merge local changes post-PR)
void idt_init(void);
void idt_install_isrs(void);
void idt_install_irqs(void);

#endif
