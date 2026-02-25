#include <arch/x86/irq.h>
#include <arch/x86/pic.h>
#include <kernel/vga.h>
#include <kernel/sched.h>
#include <kernel/shell.h>

#define PIC1_DATA_PORT 0x21
#define PIC2_DATA_PORT 0xA1
#define PIT_CMD_PORT 0x43
#define PIT_CH0_PORT 0x40
#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define PIT_BASE_HZ 1193182U

static uint32_t timer_hz_cfg = 100;
static uint32_t timer_ticks;
static uint32_t timer_seconds;

static uint8_t kbd_shift;
static uint8_t kbd_caps;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void pit_set_frequency(uint32_t hz) {
    uint32_t divisor;

    if (hz == 0) {
        hz = 100;
    }

    divisor = PIT_BASE_HZ / hz;
    if (divisor == 0) {
        divisor = 1;
    }

    outb(PIT_CMD_PORT, 0x36);
    outb(PIT_CH0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    timer_hz_cfg = hz;
}

static void print_irq_status(void) {
    uint16_t cursor_before = vga_get_cursor_pos();

    vga_write_at(80 * 24, "TIMER: ");
    vga_putdec(timer_seconds);
    vga_puts("s HZ=");
    vga_putdec(timer_hz_cfg);
    vga_puts(" T=");
    vga_putdec(sched_current_task());
    vga_puts(" SW=");
    vga_putdec(sched_switch_count());
    vga_puts("   ");

    vga_set_cursor_pos(cursor_before);
}


static uint8_t ps2_has_output(void) {
    return (uint8_t)(inb(KBD_STATUS_PORT) & 0x01U);
}

static char kbd_translate_abnt2(uint8_t scancode, uint8_t shift, uint8_t caps) {
    static const char normal[128] = {
        [0x02]='1',[0x03]='2',[0x04]='3',[0x05]='4',[0x06]='5',[0x07]='6',
        [0x08]='7',[0x09]='8',[0x0A]='9',[0x0B]='0',[0x0C]='-',[0x0D]='=',
        [0x10]='q',[0x11]='w',[0x12]='e',[0x13]='r',[0x14]='t',[0x15]='y',
        [0x16]='u',[0x17]='i',[0x18]='o',[0x19]='p',[0x1A]='[',[0x1B]=']',
        [0x1E]='a',[0x1F]='s',[0x20]='d',[0x21]='f',[0x22]='g',[0x23]='h',
        [0x24]='j',[0x25]='k',[0x26]='l',[0x27]='\x87',[0x28]='~',[0x29]='\'',
        [0x2B]='\\',[0x2C]='z',[0x2D]='x',[0x2E]='c',[0x2F]='v',[0x30]='b',
        [0x31]='n',[0x32]='m',[0x33]=',',[0x34]='.',[0x35]=';',[0x39]=' '
    };

    static const char shifted[128] = {
        [0x02]='!',[0x03]='@',[0x04]='#',[0x05]='$',[0x06]='%',[0x07]='\xa8',
        [0x08]='&',[0x09]='*',[0x0A]='(',[0x0B]=')',[0x0C]='_',[0x0D]='+',
        [0x10]='Q',[0x11]='W',[0x12]='E',[0x13]='R',[0x14]='T',[0x15]='Y',
        [0x16]='U',[0x17]='I',[0x18]='O',[0x19]='P',[0x1A]='{',[0x1B]='}',
        [0x1E]='A',[0x1F]='S',[0x20]='D',[0x21]='F',[0x22]='G',[0x23]='H',
        [0x24]='J',[0x25]='K',[0x26]='L',[0x27]='\x80',[0x28]='^',[0x29]='"',
        [0x2B]='|',[0x2C]='Z',[0x2D]='X',[0x2E]='C',[0x2F]='V',[0x30]='B',
        [0x31]='N',[0x32]='M',[0x33]='<',[0x34]='>',[0x35]=':',[0x39]=' '
    };

    char c = shift ? shifted[scancode] : normal[scancode];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
        if (caps ^ shift) {
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            else c = (char)(c - 'A' + 'a');
        }
    }
    return c;
}

static void timer_irq(void) {
    timer_ticks++;
    sched_tick();

    if ((timer_ticks % timer_hz_cfg) == 0) {
        timer_seconds++;
    }

    if ((timer_ticks % (timer_hz_cfg / 2U ? timer_hz_cfg / 2U : 1U)) == 0) {
        print_irq_status();
    }
}

static void keyboard_irq(void) {
    uint8_t scancode;

    if (!ps2_has_output()) {
        return;
    }

    scancode = inb(KBD_DATA_PORT);

    if (scancode == 0x2A || scancode == 0x36) {
        kbd_shift = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        kbd_shift = 0;
        return;
    }
    if (scancode == 0x3A) {
        kbd_caps ^= 1;
        return;
    }
    if (scancode & 0x80) {
        return;
    }

    if (scancode == 0x1C) {
        shell_on_key('\n');
        return;
    }

    if (scancode == 0x0E) {
        shell_on_key('\b');
        return;
    }

    char c = kbd_translate_abnt2(scancode, kbd_shift, kbd_caps);
    if (c != 0) {
        shell_on_key(c);
    }
}

void irq_init(uint32_t timer_hz, uint32_t scheduler_quantum_ticks) {
    pit_set_frequency(timer_hz);
    sched_init(scheduler_quantum_ticks);

    timer_ticks = 0;
    timer_seconds = 0;
    kbd_shift = 0;
    kbd_caps = 0;

    (void)inb(PIC1_DATA_PORT);
    (void)inb(PIC2_DATA_PORT);
}


uint32_t irq_timer_ticks(void) {
    return timer_ticks;
}

uint32_t irq_timer_seconds(void) {
    return timer_seconds;
}

uint32_t irq_timer_hz(void) {
    return timer_hz_cfg;
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
