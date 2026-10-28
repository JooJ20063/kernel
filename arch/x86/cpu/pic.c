#include <arch/x86/pic.h>

/* Portas I/O do PIC Master (8259A Principal) e Slave (8259A Secundário) */
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

/* Control Words de Inicialização (ICW1 e ICW4) */
#define ICW1_INIT   0x10  // Bit 4: Requer sequência de inicialização
#define ICW1_ICW4   0x01  // Bit 0: Sinaliza que a ICW4 será enviada no processo
#define ICW4_8086   0x01  // Bit 0: Opera no modo 8086/88 (em vez do modo MCS-80/85)

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * Remapeia os vetores de interrupção do PIC (por padrão 0x08..0x0F) para evitar
 * conflitos com as exceções internas da CPU x86 (0..31).
 * Geralmente chamado com offset1 = 32 (0x20) e offset2 = 40 (0x28).
 */
void pic_remap(uint8_t offset1, uint8_t offset2) {
    // Salva as máscaras de interrupção atuais (IMR) para restaurá-las ao final
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    // ICW1: Inicia a sequência de configuração em ambos os PICs
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);

    // ICW2: Define os novos vetores base da IDT para cada PIC
    outb(PIC1_DATA, offset1); // Master: IRQ 0..7  -> Vetores offset1 .. offset1+7
    outb(PIC2_DATA, offset2); // Slave:  IRQ 8..15 -> Vetores offset2 .. offset2+7

    // ICW3: Configura a conexão em cascata entre Master e Slave
    outb(PIC1_DATA, 4); // Master: Informa que há um Slave conectado no pino IRQ2 (bitmask 0000 0100)
    outb(PIC2_DATA, 2); // Slave:  Informa sua identidade em cascata ao Master (linha 2)

    // ICW4: Define parâmetros do ambiente x86
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    // Restaura as máscaras de interrupção salvas antes do remapeamento
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

/**
 * Envia o sinal EOI (End of Interrupt - 0x20) para autorizar novas IRQs.
 * Se a interrupção veio do PIC Slave (IRQ >= 8), ambos os chips precisam receber o EOI.
 */
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, 0x20);

    outb(PIC1_CMD, 0x20);
}

/* Mascara (desativa) todas as 16 IRQs nos dois controladores */
void pic_mask_all(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* Desmascara (ativa) uma IRQ específica limpando seu respectivo bit na IMR */
void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t mask;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    mask = inb(port) & (uint8_t)~(1U << irq);
    outb(port, mask);
}