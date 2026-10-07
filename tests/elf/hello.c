#include <czk/syscall.h>

static volatile unsigned int data_probe = 0x435A4B31U;
static volatile unsigned int bss_probe;

static int czk_write(int fd, const void *buffer, unsigned int size) {
    int result;

    asm volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_WRITE), "b"(fd), "c"(buffer), "d"(size)
        : "memory"
    );

    return result;
}

static int str_eq(const char *a, const char *b) {
    if (a == 0 || b == 0) {
        return 0;
    }

    while (*a != 0 && *b != 0) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == *b;
}

int main(int argc, char **argv, char **envp) {
    static const char hello[] = "Hello from ELF!\n";
    static const char exec_ok[] = "[execve] argv/envp OK\n";
    static const char startup_fail[] = "[hello] startup ABI FAILED\n";

    if (data_probe != 0x435A4B31U || bss_probe != 0U) {
        return 3;
    }

    (void)czk_write(1, hello, (unsigned int)sizeof(hello) - 1U);

    if (argc == 1 &&
        argv != 0 &&
        argv[0] != 0 &&
        envp != 0 &&
        envp[0] == 0) {
        return 0;
    }

    if (argc == 3 &&
        argv != 0 &&
        str_eq(argv[1], "alpha") &&
        str_eq(argv[2], "beta") &&
        envp != 0 &&
        str_eq(envp[0], "CZK_TEST=execve") &&
        envp[1] == 0) {
        (void)czk_write(
            1,
            exec_ok,
            (unsigned int)sizeof(exec_ok) - 1U
        );
        return 0;
    }

    (void)czk_write(
        2,
        startup_fail,
        (unsigned int)sizeof(startup_fail) - 1U
    );

    return 4;
}
