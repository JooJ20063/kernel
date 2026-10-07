#include <stdint.h>
#include <kernel/klog.h>
#include <kernel/vga.h>

static void klog_prefix(const char *lvl, uint8_t fg, uint8_t bg) {
    vga_set_color(fg, bg);
    vga_puts("[");
    vga_puts(lvl);
    vga_puts("] ");
    vga_set_color(0x0F, 0x00);
}

void klog_debug(const char *msg) {
    klog_prefix("DBG", 0x0B, 0x00);
    vga_puts(msg);
    vga_puts("\n");
}

void klog_info(const char *msg) {
    klog_prefix("INF", 0x0A, 0x00);
    vga_puts(msg);
    vga_puts("\n");
}

void klog_warn(const char *msg) {
    klog_prefix("WRN", 0x0E, 0x00);
    vga_puts(msg);
    vga_puts("\n");
}

void klog_error(const char *msg) {
    klog_prefix("ERR", 0x0C, 0x00);
    vga_puts(msg);
    vga_puts("\n");
}
