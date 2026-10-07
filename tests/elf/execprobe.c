#include <czk/syscall.h>

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

static int czk_execve(
    const char *path,
    char *const argv[],
    char *const envp[]
) {
    int result;

    asm volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(SYS_EXECVE), "b"(path), "c"(argv), "d"(envp)
        : "memory"
    );

    return result;
}

int main(int argc, char **argv, char **envp) {
    static const char before[] = "[execprobe] calling execve\n";
    static const char failed[] = "[execprobe] execve returned unexpectedly\n";
    static char path[] = "/bin/hello";
    static char arg1[] = "alpha";
    static char arg2[] = "beta";
    static char env0[] = "CZK_TEST=execve";
    char *next_argv[] = {path, arg1, arg2, 0};
    char *next_envp[] = {env0, 0};

    (void)argc;
    (void)argv;
    (void)envp;

    (void)czk_write(
        1,
        before,
        (unsigned int)sizeof(before) - 1U
    );

    (void)czk_execve(path, next_argv, next_envp);

    (void)czk_write(
        2,
        failed,
        (unsigned int)sizeof(failed) - 1U
    );

    return 90;
}
