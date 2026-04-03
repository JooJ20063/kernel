#include <arch/x86_64/idt.h>
#include <stdint.h>

#define IDT_ENTRIES 256

typedef struct idt_entry64 {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry64_t;

typedef struct idt_ptr64 {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idt_ptr64_t;

static idt_entry64_t idt[IDT_ENTRIES];
static idt_ptr64_t idt_descriptor;

static inline void lidt(const idt_ptr64_t *idt_ptr) {
    asm volatile ("lidt (%0)" : : "r"(idt_ptr) : "memory");
}

static void idt_set_gate(int n, uint64_t handler) {
    idt[n].offset_low = (uint16_t)(handler & 0xFFFFU);
    idt[n].selector = 0x08;
    idt[n].ist = 0;
    idt[n].type_attr = 0x8E;
    idt[n].offset_mid = (uint16_t)((handler >> 16) & 0xFFFFU);
    idt[n].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFFU);
    idt[n].reserved = 0;
}

void idt_init(void) {
    for (int i = 0; i < IDT_ENTRIES; ++i) {
        idt_set_gate(i, 0);
    }

    idt_descriptor.limit = (uint16_t)(sizeof(idt) - 1);
    idt_descriptor.base = (uint64_t)(uintptr_t)idt;
    lidt(&idt_descriptor);
}

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void isr128(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

void idt_install_isrs(void) {
    idt_set_gate(0, (uint64_t)(uintptr_t)isr0);
    idt_set_gate(1, (uint64_t)(uintptr_t)isr1);
    idt_set_gate(2, (uint64_t)(uintptr_t)isr2);
    idt_set_gate(3, (uint64_t)(uintptr_t)isr3);
    idt_set_gate(4, (uint64_t)(uintptr_t)isr4);
    idt_set_gate(5, (uint64_t)(uintptr_t)isr5);
    idt_set_gate(6, (uint64_t)(uintptr_t)isr6);
    idt_set_gate(7, (uint64_t)(uintptr_t)isr7);
    idt_set_gate(8, (uint64_t)(uintptr_t)isr8);
    idt_set_gate(9, (uint64_t)(uintptr_t)isr9);
    idt_set_gate(10, (uint64_t)(uintptr_t)isr10);
    idt_set_gate(11, (uint64_t)(uintptr_t)isr11);
    idt_set_gate(12, (uint64_t)(uintptr_t)isr12);
    idt_set_gate(13, (uint64_t)(uintptr_t)isr13);
    idt_set_gate(14, (uint64_t)(uintptr_t)isr14);
    idt_set_gate(15, (uint64_t)(uintptr_t)isr15);
    idt_set_gate(16, (uint64_t)(uintptr_t)isr16);
    idt_set_gate(17, (uint64_t)(uintptr_t)isr17);
    idt_set_gate(18, (uint64_t)(uintptr_t)isr18);
    idt_set_gate(19, (uint64_t)(uintptr_t)isr19);
    idt_set_gate(20, (uint64_t)(uintptr_t)isr20);
    idt_set_gate(21, (uint64_t)(uintptr_t)isr21);
    idt_set_gate(22, (uint64_t)(uintptr_t)isr22);
    idt_set_gate(23, (uint64_t)(uintptr_t)isr23);
    idt_set_gate(24, (uint64_t)(uintptr_t)isr24);
    idt_set_gate(25, (uint64_t)(uintptr_t)isr25);
    idt_set_gate(26, (uint64_t)(uintptr_t)isr26);
    idt_set_gate(27, (uint64_t)(uintptr_t)isr27);
    idt_set_gate(28, (uint64_t)(uintptr_t)isr28);
    idt_set_gate(29, (uint64_t)(uintptr_t)isr29);
    idt_set_gate(30, (uint64_t)(uintptr_t)isr30);
    idt_set_gate(31, (uint64_t)(uintptr_t)isr31);
    idt_set_gate(128, (uint64_t)(uintptr_t)isr128);
}

void idt_install_irqs(void) {
    idt_set_gate(32, (uint64_t)(uintptr_t)irq0);
    idt_set_gate(33, (uint64_t)(uintptr_t)irq1);
    idt_set_gate(34, (uint64_t)(uintptr_t)irq2);
    idt_set_gate(35, (uint64_t)(uintptr_t)irq3);
    idt_set_gate(36, (uint64_t)(uintptr_t)irq4);
    idt_set_gate(37, (uint64_t)(uintptr_t)irq5);
    idt_set_gate(38, (uint64_t)(uintptr_t)irq6);
    idt_set_gate(39, (uint64_t)(uintptr_t)irq7);
    idt_set_gate(40, (uint64_t)(uintptr_t)irq8);
    idt_set_gate(41, (uint64_t)(uintptr_t)irq9);
    idt_set_gate(42, (uint64_t)(uintptr_t)irq10);
    idt_set_gate(43, (uint64_t)(uintptr_t)irq11);
    idt_set_gate(44, (uint64_t)(uintptr_t)irq12);
    idt_set_gate(45, (uint64_t)(uintptr_t)irq13);
    idt_set_gate(46, (uint64_t)(uintptr_t)irq14);
    idt_set_gate(47, (uint64_t)(uintptr_t)irq15);
}
