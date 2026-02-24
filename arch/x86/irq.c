#include <arch/x86/irq.h>
#include <arch/x86/pic.h>
#include <kernel/vga.h>

#define KBD_DATA_PORT 0x60
#define TIMER_HZ 18

static uint32_t timer_ticks;
static uint32_t timer_seconds;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static char kbd_scancode_to_char(uint8_t scancode) {
    static const char keymap[] = {
        0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z',
        'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
    };

    if (scancode < sizeof(keymap)) {
        return keymap[scancode];
    }

    return 0;
}

static void timer_irq(void) {
    timer_ticks++;

    if ((timer_ticks % TIMER_HZ) == 0) {
        timer_seconds++;
        vga_write_at(80 * 24, "TIMER: ");
        vga_putdec(timer_seconds);
        vga_puts("s   ");
    }
}

static void keyboard_irq(void) {
    uint8_t scancode = inb(KBD_DATA_PORT);

    if (scancode & 0x80) {
        return;
    }

    vga_write_at(80 * 23, "KEY: ");

    if (scancode == 0x1C) {
        vga_puts("ENTER   ");
        return;
    }

    if (scancode == 0x0E) {
        vga_puts("BACKSPACE");
        return;
    }

    char c = kbd_scancode_to_char(scancode);
    if (c != 0) {
        vga_putc(c);
        vga_puts("      ");
    } else {
        vga_puts("SC=");
        vga_puthex(scancode);
    }
}

void irq_handler_c(registers_t *regs) {
    if (regs->int_no < 32 || regs->int_no > 47) {
        return;
    }

    switch (regs->int_no - 32) {
        case 0:
            timer_irq();
            break;
        case 1:
            keyboard_irq();
            break;
        default:
            break;
    }

    pic_send_eoi(regs->int_no - 32);
}
