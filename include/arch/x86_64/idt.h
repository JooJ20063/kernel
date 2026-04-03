#ifndef ARCH_X86_64_IDT_H
#define ARCH_X86_64_IDT_H

void idt_init(void);
void idt_install_isrs(void);
void idt_install_irqs(void);

#endif
