#ifndef KERNEL_SERIAL_H
#define KERNEL_SERIAL_H

#include <stdint.h>

void serial_init(void);
int serial_is_available(void);
void serial_enable_rx_interrupt(void);

void serial_putc(char c);
void serial_puts(const char *s);

int serial_rx_ready(void);
char serial_getc(void);

#endif
