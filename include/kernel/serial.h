#ifndef KERNEL_SERIAL_H
#define KERNEL_SERIAL_H

#include <stdint.h>

/* Inicializa a porta serial (COM1), ajusta a taxa de baud para 115200bps e executa o teste de loopback */
void serial_init(void);

/* Retorna 1 se a porta serial passou no teste de hardware e está operacional; caso contrário, retorna 0 */
int serial_is_available(void);

/* Habilita a geração de interrupção de entrada (IRQ4) ao receber dados no buffer da UART */
void serial_enable_rx_interrupt(void);

/* Envia um único caractere pela porta serial (bloqueante via busy-wait) */
void serial_putc(char c);

/* Envia uma string terminada em nulo pela porta serial */
void serial_puts(const char *s);

/* Verifica se existe ao menos um byte disponível no buffer de recepção da UART */
int serial_rx_ready(void);

/* Lê e retorna um byte do buffer de entrada (retorna 0 se o buffer estiver vazio) */
char serial_getc(void);

#endif