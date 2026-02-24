#include <kernel/vga.h>

#define VGA_MEM ((volatile uint8_t *)0xB8000)
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_SIZE (VGA_WIDTH * VGA_HEIGHT)

#define CRT_INDEX_PORT 0x3D4
#define CRT_DATA_PORT 0x3D5

static uint16_t cursor;
static uint8_t color = 0x0F;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void vga_sync_hw_cursor(void) {
    outb(CRT_INDEX_PORT, 0x0F);
    outb(CRT_DATA_PORT, (uint8_t)(cursor & 0xFF));
    outb(CRT_INDEX_PORT, 0x0E);
    outb(CRT_DATA_PORT, (uint8_t)((cursor >> 8) & 0xFF));
}

static void vga_scroll_if_needed(void) {
    if (cursor < VGA_SIZE) {
        return;
    }

    for (uint16_t row = 1; row < VGA_HEIGHT; ++row) {
        for (uint16_t col = 0; col < VGA_WIDTH; ++col) {
            uint16_t from = row * VGA_WIDTH + col;
            uint16_t to = (row - 1) * VGA_WIDTH + col;
            VGA_MEM[to * 2] = VGA_MEM[from * 2];
            VGA_MEM[to * 2 + 1] = VGA_MEM[from * 2 + 1];
        }
    }

    for (uint16_t col = 0; col < VGA_WIDTH; ++col) {
        uint16_t cell = (VGA_HEIGHT - 1) * VGA_WIDTH + col;
        VGA_MEM[cell * 2] = ' ';
        VGA_MEM[cell * 2 + 1] = color;
    }

    cursor = (VGA_HEIGHT - 1) * VGA_WIDTH;
}

uint16_t vga_get_cursor_pos(void) {
    return cursor;
}

void vga_set_cursor_pos(uint16_t pos) {
    cursor = pos;
    if (cursor >= VGA_SIZE) {
        cursor = VGA_SIZE - 1;
    }
    vga_sync_hw_cursor();
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    color = (uint8_t)((bg << 4) | (fg & 0x0F));
}

void vga_clear(void) {
    for (uint16_t i = 0; i < VGA_SIZE; ++i) {
        VGA_MEM[i * 2] = ' ';
        VGA_MEM[i * 2 + 1] = color;
    }
    cursor = 0;
    vga_sync_hw_cursor();
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor = (uint16_t)(((cursor / VGA_WIDTH) + 1) * VGA_WIDTH);
        vga_scroll_if_needed();
        vga_sync_hw_cursor();
        return;
    }

    if (c == '\r') {
        cursor = (uint16_t)((cursor / VGA_WIDTH) * VGA_WIDTH);
        vga_sync_hw_cursor();
        return;
    }

    VGA_MEM[cursor * 2] = (uint8_t)c;
    VGA_MEM[cursor * 2 + 1] = color;
    cursor++;
    vga_scroll_if_needed();
    vga_sync_hw_cursor();
}

void vga_puts(const char *s) {
    while (*s) {
        vga_putc(*s++);
    }
}

void vga_puthex(uint32_t value) {
    static const char *hex = "0123456789ABCDEF";

    vga_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        vga_putc(hex[(value >> shift) & 0xF]);
    }
}

void vga_putdec(uint32_t value) {
    char buf[10];
    int i = 0;

    if (value == 0) {
        vga_putc('0');
        return;
    }

    while (value > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (i > 0) {
        vga_putc(buf[--i]);
    }
}

void vga_write_at(uint16_t pos, const char *s) {
    vga_set_cursor_pos(pos);
    vga_puts(s);
}
