static volatile unsigned int data_probe = 0x435A4B31U;
static volatile unsigned int bss_probe;

static int czk_write(int fd, const void *buffer, unsigned int size) {
    int result;

    asm volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(1), "b"(fd), "c"(buffer), "d"(size)
        : "memory"
    );

    return result;
}

__attribute__((noreturn))
static void czk_exit(int code) {
    asm volatile (
        "int $0x80"
        :
        : "a"(2), "b"(code)
        : "memory"
    );

    for (;;) {
        asm volatile ("pause");
    }
}

int main(void) {
    static const char message[] = "Hello from ELF!\n";

    if (data_probe != 0x435A4B31U || bss_probe != 0U) {
        czk_exit(3);
    }

    (void)czk_write(1, message, 16U);
    czk_exit(0);
}

__attribute__((noreturn))
void _start(void) {
    (void)main();
    czk_exit(0);
}
