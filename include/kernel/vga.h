#pragma once

#include <stdint.h>

/* Controle do posicionamento do cursor de hardware da placa VGA */
void vga_set_cursor_pos(uint16_t pos);
uint16_t vga_get_cursor_pos(void);

/* Configura as cores de texto (foreground) e fundo (background) */
void vga_set_color(uint8_t fg, uint8_t bg);

/* Limpa a tela preenchendo a VRAM com espaços e reseta o cursor */
void vga_clear(void);

/* Saída de caracteres e strings */
void vga_putc(char c);
void vga_puts(const char *s);

/* Formatação e exibição de valores numéricos */
void vga_puthex(uint32_t value);
void vga_puthex64(uint64_t value);
void vga_putdec(uint32_t value);

/* Escreve uma string em um deslocamento específico da memória de vídeo */
void vga_write_at(uint16_t pos, const char *s);

/* Alterna o espelhamento da saída de texto para a porta serial COM1 */
void vga_set_serial_mirror(int enabled);