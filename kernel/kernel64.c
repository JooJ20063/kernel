#include <stddef.h>

/* VGA Buffer */
#define VGA_ADDRESS 0xB8000
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_SIZE    (VGA_WIDTH * VGA_HEIGHT)

static volatile unsigned short *vga_buffer = (volatile unsigned short *)VGA_ADDRESS;
static size_t vga_row = 0;
static size_t vga_col = 0;
static unsigned char vga_color = 0x0F;

static void vga_clear(void) {
    for (size_t i = 0; i < VGA_SIZE; i++) {
        vga_buffer[i] = (vga_color << 8) | ' ';
    }
    vga_row = 0;
    vga_col = 0;
}

static void vga_putc(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) {
            vga_row = VGA_HEIGHT - 1;
            for (size_t i = 0; i < (VGA_SIZE - VGA_WIDTH); i++) {
                vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
            }
            for (size_t i = (VGA_SIZE - VGA_WIDTH); i < VGA_SIZE; i++) {
                vga_buffer[i] = (vga_color << 8) | ' ';
            }
        }
        return;
    }
    size_t idx = vga_row * VGA_WIDTH + vga_col;
    if (idx < VGA_SIZE) {
        vga_buffer[idx] = (vga_color << 8) | c;
    }
    vga_col++;
    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) {
            vga_row = VGA_HEIGHT - 1;
            for (size_t i = 0; i < (VGA_SIZE - VGA_WIDTH); i++) {
                vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
            }
            for (size_t i = (VGA_SIZE - VGA_WIDTH); i < VGA_SIZE; i++) {
                vga_buffer[i] = (vga_color << 8) | ' ';
            }
        }
    }
}

static void vga_puts(const char *s) {
    while (*s) {
        vga_putc(*s++);
    }
}

void kernel_main64(void) {
    vga_clear();
    vga_puts("=== Kernel 64 Long Mode ===\n");
    vga_puts("Welcome to x86_64 kernel!\n");
    vga_puts("arch: x86_64\n");
    vga_puts("Commands: help, clear, arch, shutdown\n\n");

    vga_puts("kernel64> ");

    while (1) {
        __asm__ volatile ("hlt");
    }
}
