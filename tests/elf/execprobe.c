#include <czk/syscall.h>

/*
 * Solicita a escrita de 'size' bytes no descritor 'fd'.
 * ABI x86: EAX = syscall; EBX, ECX e EDX = argumentos.
 * Retorna o resultado fornecido pelo kernel.
 */
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

/*
 * Solicita a substituição da imagem do processo pelo executável em 'path'.
 * argv e envp são vetores de ponteiros terminados por um ponteiro nulo.
 * Em caso de sucesso, a execução continua na nova imagem e não retorna aqui.
 * Em caso de falha, retorna um código de erro negativo.
 */
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

/*
 * Teste de integração de execve:
 * substitui este programa por /bin/hello, fornecendo argumentos e ambiente
 * para verificar a construção da pilha inicial do novo executável.
 */
int main(int argc, char **argv, char **envp) {
    static const char before[] = "[execprobe] calling execve\n";
    static const char failed[] = "[execprobe] execve returned unexpectedly\n";

    /* Strings que serão copiadas pelo kernel antes da troca de imagem. */
    static char path[] = "/bin/hello";
    static char arg1[] = "alpha";
    static char arg2[] = "beta";
    static char env0[] = "CZK_TEST=execve";

    /* A nova imagem recebe argc = 3, argv[0] = caminho do executável. */
    char *next_argv[] = {path, arg1, arg2, 0};
    char *next_envp[] = {env0, 0};

    /* Este teste não utiliza os argumentos recebidos na imagem original. */
    (void)argc;
    (void)argv;
    (void)envp;

    /* Emite a mensagem em stdout, excluindo o terminador NUL da string. */
    (void)czk_write(
        1,
        before,
        (unsigned int)sizeof(before) - 1U
    );

    /* O sucesso desta chamada impede a execução das instruções seguintes. */
    (void)czk_execve(path, next_argv, next_envp);

    /* Se execve retornar, informa a falha em stderr. */
    (void)czk_write(
        2,
        failed,
        (unsigned int)sizeof(failed) - 1U
    );

    /* O crt0 encaminha este retorno para a syscall de saída. */
    return 90;
}