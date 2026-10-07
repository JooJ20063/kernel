#include <arch/x86/idt.h>
#include <stdint.h>

extern void isr_default(void);

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_descriptor;

static inline void lidt(struct idt_ptr* idt_ptr){
    asm volatile ("lidt (%0)" : : "r"(idt_ptr));
}

static void idt_set_gate(int n, uint32_t handler, uint8_t type_attr) {
    idt[n].offset_low   = handler & 0xFFFF;
    idt[n].selector     = 0x08;
    idt[n].zero         = 0;
    idt[n].type_attr    = type_attr;
    idt[n].offset_high  = (handler >> 16) & 0xFFFF;
}

void idt_init(void) {
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, (uint32_t)isr_default, 0x8E);
    }

    idt_descriptor.limit  = sizeof(idt) - 1;
    idt_descriptor.base   = (uint32_t)idt;
    lidt(&idt_descriptor);
}

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

extern void isr128();

void idt_install_isrs(void) {
    idt_set_gate(0, (uint32_t)isr0, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x8E);
    idt_set_gate(2, (uint32_t)isr2, 0x8E);
    idt_set_gate(3, (uint32_t)isr3, 0x8E);
    idt_set_gate(4, (uint32_t)isr4, 0x8E);
    idt_set_gate(5, (uint32_t)isr5, 0x8E);
    idt_set_gate(6, (uint32_t)isr6, 0x8E);
    idt_set_gate(7, (uint32_t)isr7, 0x8E);
    idt_set_gate(8, (uint32_t)isr8, 0x8E);
    idt_set_gate(9, (uint32_t)isr9, 0x8E);
    idt_set_gate(10, (uint32_t)isr10, 0x8E);
    idt_set_gate(11, (uint32_t)isr11, 0x8E);
    idt_set_gate(12, (uint32_t)isr12, 0x8E);
    idt_set_gate(13, (uint32_t)isr13, 0x8E);
    idt_set_gate(14, (uint32_t)isr14, 0x8E);
    idt_set_gate(15, (uint32_t)isr15, 0x8E);
    idt_set_gate(16, (uint32_t)isr16, 0x8E);
    idt_set_gate(17, (uint32_t)isr17, 0x8E);
    idt_set_gate(18, (uint32_t)isr18, 0x8E);
    idt_set_gate(19, (uint32_t)isr19, 0x8E);
    idt_set_gate(20, (uint32_t)isr20, 0x8E);
    idt_set_gate(21, (uint32_t)isr21, 0x8E);
    idt_set_gate(22, (uint32_t)isr22, 0x8E);
    idt_set_gate(23, (uint32_t)isr23, 0x8E);
    idt_set_gate(24, (uint32_t)isr24, 0x8E);
    idt_set_gate(25, (uint32_t)isr25, 0x8E);
    idt_set_gate(26, (uint32_t)isr26, 0x8E);
    idt_set_gate(27, (uint32_t)isr27, 0x8E);
    idt_set_gate(28, (uint32_t)isr28, 0x8E);
    idt_set_gate(29, (uint32_t)isr29, 0x8E);
    idt_set_gate(30, (uint32_t)isr30, 0x8E);
    idt_set_gate(31, (uint32_t)isr31, 0x8E);
    idt_set_gate(128, (uint32_t)isr128, 0xEE);
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
extern void irq13();
extern void irq14();
extern void irq15();
extern void irq_yield();

void idt_install_irqs(void) {
    idt_set_gate(32, (uint32_t)irq0, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x8E);
    idt_set_gate(34, (uint32_t)irq2, 0x8E);
    idt_set_gate(35, (uint32_t)irq3, 0x8E);
    idt_set_gate(36, (uint32_t)irq4, 0x8E);
    idt_set_gate(37, (uint32_t)irq5, 0x8E);
    idt_set_gate(38, (uint32_t)irq6, 0x8E);
    idt_set_gate(39, (uint32_t)irq7, 0x8E);
    idt_set_gate(40, (uint32_t)irq8, 0x8E);
    idt_set_gate(41, (uint32_t)irq9, 0x8E);
    idt_set_gate(42, (uint32_t)irq10, 0x8E);
    idt_set_gate(43, (uint32_t)irq11, 0x8E);
    idt_set_gate(44, (uint32_t)irq12, 0x8E);
    idt_set_gate(45, (uint32_t)irq13, 0x8E);
    idt_set_gate(46, (uint32_t)irq14, 0x8E);
    idt_set_gate(47, (uint32_t)irq15, 0x8E);

    idt_set_gate(129, (uint32_t)irq_yield, 0x8E);
}
