<<<<<<< HEAD
#include <arch/x86_64/pic.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20
=======
#include <arch/x86/pic.h>

#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

#define ICW1_INIT   0x10
#define ICW1_ICW4   0x01
#define ICW4_8086   0x01
>>>>>>> 4cc05da (Merge local changes post-PR)

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

<<<<<<< HEAD
void pic_remap(int offset1, int offset2) {
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    outb(PIC1_DATA, (uint8_t)offset1);
    outb(PIC2_DATA, (uint8_t)offset2);
=======
void pic_remap(uint8_t offset1, uint8_t offset2) {
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

    outb(PIC1_DATA, offset1);
    outb(PIC2_DATA, offset2);
>>>>>>> 4cc05da (Merge local changes post-PR)

    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

<<<<<<< HEAD
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
=======
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);
>>>>>>> 4cc05da (Merge local changes post-PR)

    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
<<<<<<< HEAD
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
=======
        outb(PIC2_CMD, 0x20);
    }
    outb(PIC1_CMD, 0x20);
>>>>>>> 4cc05da (Merge local changes post-PR)
}

void pic_mask_all(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t mask;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
<<<<<<< HEAD
        irq = (uint8_t)(irq - 8);
    }

    mask = (uint8_t)(inb(port) & (uint8_t)~(1U << irq));
=======
        irq -= 8;
    }

    mask = inb(port) & (uint8_t)~(1U << irq);
>>>>>>> 4cc05da (Merge local changes post-PR)
    outb(port, mask);
}
