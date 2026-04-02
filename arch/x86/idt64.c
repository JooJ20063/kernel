#include <arch/x86/idt64.h>

#define IDT_ENTRIES 256

static struct idt_entry idt64[IDT_ENTRIES] __attribute__((aligned(16)));
static struct idt_ptr idt_descriptor64;

static void lidt64(struct idt_ptr *idt_ptr) {
    __asm__ volatile ("lidt (%0)" : : "r"(idt_ptr) : "memory");
}

static void idt_set_gate64(int n, unsigned long handler, unsigned char ist_value) {
    idt64[n].offset_low = handler & 0xFFFF;
    idt64[n].selector = 0x08;
    idt64[n].ist = ist_value;
    idt64[n].type_attr = 0x8E;
    idt64[n].offset_mid = (handler >> 16) & 0xFFFF;
    idt64[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt64[n].reserved = 0;
}

void idt_init64(void) {
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate64(i, 0, 0);
    }

    idt_descriptor64.limit = sizeof(idt64) - 1;
    idt_descriptor64.base = (unsigned long)idt64;
    lidt64(&idt_descriptor64);
}
