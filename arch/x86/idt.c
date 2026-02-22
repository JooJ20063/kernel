#include <arch/x86/idt.h>
#include <stdint.h>

extern void isr_default(void);

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_descriptor;

static inline void lidt(struct idt_ptr* idt_ptr){
	asm volatile ("lidt (%0)" : : "r"(idt_ptr));
}

static void idt_set_gate(int n, uint32_t handler) {
    idt[n].offset_low   = handler & 0xFFFF;
    idt[n].selector     = 0x08;
    idt[n].zero         = 0;
    idt[n].type_attr    = 0x8E;
    idt[n].offset_high  = (handler >> 16) & 0xFFFF;
}

void idt_init(void) {
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)isr_default);
    }
    idt_descriptor.limit  = sizeof(idt) - 1;
    idt_descriptor.base   = (uint32_t)idt;

    lidt(&idt_descriptor);
}

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

void idt_install_irqs(void) {
  idt_set_gate(32, (uint32_t)irq0);
  idt_set_gate(33, (uint32_t)irq1);
  idt_set_gate(34, (uint32_t)irq2);
  idt_set_gate(35, (uint32_t)irq3);
  idt_set_gate(36, (uint32_t)irq4);
  idt_set_gate(37, (uint32_t)irq5);
  idt_set_gate(38, (uint32_t)irq6);
  idt_set_gate(39, (uint32_t)irq7);
  idt_set_gate(40, (uint32_t)irq8);
  idt_set_gate(41, (uint32_t)irq9);
  idt_set_gate(42, (uint32_t)irq10);
  idt_set_gate(43, (uint32_t)irq11);
  idt_set_gate(44, (uint32_t)irq12);
  idt_set_gate(45, (uint32_t)irq13);
  idt_set_gate(46, (uint32_t)irq14);
  idt_set_gate(47, (uint32_t)irq15);
}
