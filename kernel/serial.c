#include <kernel/serial.h>

#define COM1_BASE 0x3F8

#define UART_DATA        0
#define UART_IER         1
#define UART_FIFO_CTRL   2
#define UART_LINE_CTRL   3
#define UART_MODEM_CTRL  4
#define UART_LINE_STATUS 5

#define UART_LSR_DATA_READY 0x01
#define UART_LSR_TX_EMPTY   0x20

static int serial_available;

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_init(void) {
    serial_available = 0;

    outb(COM1_BASE + UART_IER, 0x00);

    outb(COM1_BASE + UART_LINE_CTRL, 0x80);
    outb(COM1_BASE + UART_DATA, 0x01);
    outb(COM1_BASE + UART_IER, 0x00);

    outb(COM1_BASE + UART_LINE_CTRL, 0x03);
    outb(COM1_BASE + UART_FIFO_CTRL, 0xC7);

    outb(COM1_BASE + UART_MODEM_CTRL, 0x1E);
    outb(COM1_BASE + UART_DATA, 0xAE);

    if (inb(COM1_BASE + UART_DATA) != 0xAE) {
        outb(COM1_BASE + UART_MODEM_CTRL, 0x00);
        return;
    }

    outb(COM1_BASE + UART_MODEM_CTRL, 0x0B);
    serial_available = 1;
}

int serial_is_available(void) {
    return serial_available;
}

void serial_enable_rx_interrupt(void) {
    if (!serial_available) {
        return;
    }

    outb(COM1_BASE + UART_IER, 0x01);
}

void serial_putc(char c) {
    if (!serial_available) {
        return;
    }

    if (c == '\n') {
        serial_putc('\r');
    }

    while ((inb(COM1_BASE + UART_LINE_STATUS) & UART_LSR_TX_EMPTY) == 0) {
    }

    outb(COM1_BASE + UART_DATA, (uint8_t)c);
}

void serial_puts(const char *s) {
    if (!serial_available || s == 0) {
        return;
    }

    while (*s != 0) {
        serial_putc(*s++);
    }
}

int serial_rx_ready(void) {
    if (!serial_available) {
        return 0;
    }

    return (inb(COM1_BASE + UART_LINE_STATUS) & UART_LSR_DATA_READY) != 0;
}

char serial_getc(void) {
    if (!serial_rx_ready()) {
        return 0;
    }

    return (char)inb(COM1_BASE + UART_DATA);
}
