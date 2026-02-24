#include <kernel/vga.h>

#define VGA_BUFFER ((volatile char*)0xB8000)
#define VGA_ATTR_DEFAULT 0x0F
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

static uint16_t cursor;

void vga_set_cursor_pos(uint16_t pos) {
    cursor = pos;
}

void vga_clear(void) {
    for (uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) {
        VGA_BUFFER[i * 2] = ' ';
        VGA_BUFFER[i * 2 + 1] = VGA_ATTR_DEFAULT;
    }
    cursor = 0;
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor = (uint16_t)(((cursor / VGA_WIDTH) + 1) * VGA_WIDTH);
        return;
    }

    VGA_BUFFER[cursor * 2] = c;
    VGA_BUFFER[cursor * 2 + 1] = VGA_ATTR_DEFAULT;
    cursor++;
}

void vga_puts(const char *s) {
    while (*s) {
        vga_putc(*s++);
    }
}

void vga_puthex(uint32_t value) {
    static const char* hex = "0123456789ABCDEF";

    vga_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        vga_putc(hex[(value >> shift) & 0xF]);
    }
}

void vga_write_at(uint16_t pos, const char *s) {
    vga_set_cursor_pos(pos);
    vga_puts(s);
}
