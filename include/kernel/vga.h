#pragma once

#include <stdint.h>

void vga_set_cursor_pos(uint16_t pos);
void vga_clear(void);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_puthex(uint32_t value);
void vga_write_at(uint16_t pos, const char *s);
