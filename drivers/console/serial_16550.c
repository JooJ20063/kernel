#include <kernel/serial.h>

/* Endereço I/O base da primeira porta serial (COM1) */
#define COM1_BASE 0x3F8

/* Offsets dos registradores do controlador UART 16550 */
#define UART_DATA        0  // Buffer de Transmissão/Recepção (RBR/THR) e LSB do Divisor de Baud (se DLAB=1)
#define UART_IER         1  // Interrupt Enable Register e MSB do Divisor de Baud (se DLAB=1)
#define UART_FIFO_CTRL   2  // FIFO Control Register (FCR)
#define UART_LINE_CTRL   3  // Line Control Register (LCR)
#define UART_MODEM_CTRL  4  // Modem Control Register (MCR)
#define UART_LINE_STATUS 5  // Line Status Register (LSR)

/* Máscaras de bits do registrador de status da linha (LSR) */
#define UART_LSR_DATA_READY 0x01  // Bit 0: Byte disponível para leitura no buffer de entrada
#define UART_LSR_TX_EMPTY   0x20  // Bit 5: Buffer de transmissão livre para envio de novos dados

static int serial_available;

/* Wrappers inline para I/O em portas do x86 */
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/**
 * Inicializa a UART 16550 (COM1), ajusta a taxa de baud para 115200bps,
 * habilita os buffers FIFO e realiza o teste de hardware (Loopback Test).
 */
void serial_init(void) {
    serial_available = 0;

    // Desativa temporariamente todas as interrupções do chip UART
    outb(COM1_BASE + UART_IER, 0x00);

    // Ativa DLAB (Bit 7 = 1 no LCR) para permitir configuração dos registradores de divisor de baud
    outb(COM1_BASE + UART_LINE_CTRL, 0x80);
    outb(COM1_BASE + UART_DATA, 0x01); // Divisor LSB = 1 (115200 baud)
    outb(COM1_BASE + UART_IER, 0x00);  // Divisor MSB = 0

    // DLAB = 0, 8 bits de dados, sem paridade, 1 stop bit (8N1: modo padrão de comunicação)
    outb(COM1_BASE + UART_LINE_CTRL, 0x03);

    // Habilita FIFO, limpa buffers RX/TX e define gatilho de interrupção para 14 bytes (0xC7)
    outb(COM1_BASE + UART_FIFO_CTRL, 0xC7);

    /* 
     * Teste de Loopback (MCR = 0x1E):
     * Coloca o chip em modo de teste local (Bit 4) para verificar a saúde do hardware.
     */
    outb(COM1_BASE + UART_MODEM_CTRL, 0x1E);
    outb(COM1_BASE + UART_DATA, 0xAE); // Envia byte de teste

    // Se o dado lido for diferente do enviado, a porta serial é considerada ausente ou defeituosa
    if (inb(COM1_BASE + UART_DATA) != 0xAE) {
        outb(COM1_BASE + UART_MODEM_CTRL, 0x00);
        return;
    }

    // Desativa modo de Loopback e ativa sinais RTS/DTR/Out2 (0x0B) para operação normal
    outb(COM1_BASE + UART_MODEM_CTRL, 0x0B);
    serial_available = 1;
}

int serial_is_available(void) {
    return serial_available;
}

/* Habilita a geração de IRQ4 ao receber dados no buffer de recepção (Bit 0 no IER) */
void serial_enable_rx_interrupt(void) {
    if (!serial_available) {
        return;
    }

    outb(COM1_BASE + UART_IER, 0x01);
}

/* Envia um caractere via serial de forma bloqueante (busy-wait), convertendo LF em CRLF */
void serial_putc(char c) {
    if (!serial_available) {
        return;
    }

    if (c == '\n') {
        serial_putc('\r');
    }

    // Aguarda o registrador de transmissão (THR) ficar livre
    while ((inb(COM1_BASE + UART_LINE_STATUS) & UART_LSR_TX_EMPTY) == 0) {
    }

    outb(COM1_BASE + UART_DATA, (uint8_t)c);
}

/* Escreve uma string terminada em nulo na porta serial */
void serial_puts(const char *s) {
    if (!serial_available || s == 0) {
        return;
    }

    while (*s != 0) {
        serial_putc(*s++);
    }
}

/* Checa se existe ao menos 1 byte pendente de leitura no buffer de entrada */
int serial_rx_ready(void) {
    if (!serial_available) {
        return 0;
    }

    return (inb(COM1_BASE + UART_LINE_STATUS) & UART_LSR_DATA_READY) != 0;
}

/* Lê um byte do buffer de recepção da UART (retorna 0 se o buffer estiver vazio) */
char serial_getc(void) {
    if (!serial_rx_ready()) {
        return 0;
    }

    return (char)inb(COM1_BASE + UART_DATA);
}