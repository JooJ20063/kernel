#include <stdint.h>

#include <czk/abi.h>
#include <czk/syscall.h>
#include <czk/errno.h>
#include <czk/fcntl.h>
#include <czk/fs.h>
#include <czk/stat.h>
#include <czk/dirent.h>

_Static_assert(sizeof(czk_stat_t) == 8U, "stat ABI mismatch");
_Static_assert(sizeof(czk_dirent_t) == 136U, "dirent ABI mismatch");

static int32_t syscall0(uint32_t number) {
    int32_t result;

    asm volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(number)
        : "memory"
    );

    return result;
}

static int32_t syscall1(uint32_t number, uint32_t arg1) {
    int32_t result;

    asm volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(arg1)
        : "memory"
    );

    return result;
}

static int32_t syscall2(
    uint32_t number,
    uint32_t arg1,
    uint32_t arg2
) {
    int32_t result;

    asm volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(arg1), "c"(arg2)
        : "memory"
    );

    return result;
}

static int32_t syscall3(
    uint32_t number,
    uint32_t arg1,
    uint32_t arg2,
    uint32_t arg3
) {
    int32_t result;

    asm volatile (
        "int $0x80"
        : "=a"(result)
        : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );

    return result;
}

static void print(const char *text, uint32_t size) {
    (void)syscall3(
        CZK_SYS_WRITE,
        CZK_STDOUT_FILENO,
        (uint32_t)(uintptr_t)text,
        size
    );
}

int main(int argc, char **argv, char **envp) {
    static const char path[] = "/bin/hello";
    static const char ok[] = "[abi] CZK x86 ABI v1.0 OK\n";
    static const char fail[] = "[abi] ABI v1.0 probe FAILED\n";
    czk_stat_t stat;
    int32_t fd;

    if (argc < 1 || argv == 0 || envp == 0) {
        goto failed;
    }

    if ((uint32_t)syscall0(CZK_SYS_ABI_VERSION) !=
        CZK_ABI_VERSION_CURRENT) {
        goto failed;
    }

    if (syscall0(CZK_SYS_YIELD) != 0) {
        goto failed;
    }

    if (syscall0(0x7FFFFFFFU) != -CZK_ENOSYS) {
        goto failed;
    }

    fd = syscall2(
        CZK_SYS_OPEN,
        (uint32_t)(uintptr_t)path,
        CZK_O_RDONLY
    );

    if (fd < 0) {
        goto failed;
    }

    if (syscall2(
            CZK_SYS_FSTAT,
            (uint32_t)fd,
            (uint32_t)(uintptr_t)&stat) != 0) {
        (void)syscall1(CZK_SYS_CLOSE, (uint32_t)fd);
        goto failed;
    }

    if ((stat.st_flags & CZK_FS_FILE) == 0U ||
        stat.st_size == 0U) {
        (void)syscall1(CZK_SYS_CLOSE, (uint32_t)fd);
        goto failed;
    }

    if (syscall1(CZK_SYS_CLOSE, (uint32_t)fd) != 0) {
        goto failed;
    }

    print(ok, (uint32_t)sizeof(ok) - 1U);
    return 0;

failed:
    print(fail, (uint32_t)sizeof(fail) - 1U);
    return 1;
}
