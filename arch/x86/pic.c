#include <arch/x86/pic.h>

//portas

#define PIC1_CMD	0x20
#define PIC1_DATA	0x21
#define PIC2_CMD	0xA0
#define PIC2_DATA	0xA1

#define ICW1_INIT	0x10
#define ICW1_ICW4	0x01
#define ICW4_8086	0x01

static inline void outb(uint16_t port, uint8_t val) {
	asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
	uint8_t ret;
	asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

void pic_remap(uint8_t offset1, uint8_t offset2) {
	uint8_t a1 = inb(PIC1_DATA);
	uint8_t a2 = inb(PIC2_DATA);

	//Inicio da Inicialização
	outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
	outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

	//Novos offsets
	outb(PIC1_DATA, offset1);
	outb(PIC2_DATA, offset2);

	//Cascata
	outb(PIC1_DATA, 4);
	outb(PIC2_DATA, 2);

	//8086
	outb(PIC1_DATA, ICW4_8086);
	outb(PIC2_DATA, ICW4_8086);

	//Restaura Mascaras
	outb(PIC1_DATA, a1);
	outb(PIC2_DATA, a2);
}

void pic_send_eoi(uint8_t irq) {
	if (irq >= 8)
		outb(PIC2_CMD, 0x20);
	
	outb(PIC1_CMD, 0x20);
}
