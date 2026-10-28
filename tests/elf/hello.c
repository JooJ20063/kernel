#include <czk/syscall.h>

/*
 * Sondas para verificar a inicialização da imagem pelo carregador ELF.
 * data_probe verifica os dados inicializados; bss_probe deve começar zerada.
 * volatile mantém os acessos observáveis, evitando sua eliminação pelo compilador.
 */
static volatile unsigned int data_probe = 0x435A4B31U;
static volatile unsigned int bss_probe;

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
 * Compara duas strings terminadas em NUL.
 * Retorna 1 se forem iguais ou 0 se diferirem ou algum ponteiro for nulo.
 */
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

/*
 * Testa o carregamento ELF e os argumentos entregues pelo crt0.
 * Aceita dois cenários: execução direta ou substituição via execprobe.
 *
 * Códigos de saída:
 *   0 — verificações realizadas com sucesso;
 *   3 — falha na inicialização dos dados ou da BSS;
 *   4 — argumentos ou ambiente diferentes dos cenários esperados.
 */
int main(int argc, char **argv, char **envp) {
    static const char hello[] = "Hello from ELF!\n";
    static const char exec_ok[] = "[execve] argv/envp OK\n";
    static const char startup_fail[] = "[hello] startup ABI FAILED\n";

    /* Confirma a preservação dos dados inicializados e o zeramento da BSS. */
    if (data_probe != 0x435A4B31U || bss_probe != 0U) {
        return 3;
    }

    /* Emite a mensagem em stdout, sem incluir o terminador NUL. */
    (void)czk_write(1, hello, (unsigned int)sizeof(hello) - 1U);

    /*
     * Execução direta: espera um argumento e ambiente vazio.
     * Neste cenário, verifica apenas a existência de argv[0].
     */
    if (argc == 1 &&
        argv != 0 &&
        argv[0] != 0 &&
        envp != 0 &&
        envp[0] == 0) {
        return 0;
    }

    /*
     * Execução via execprobe: verifica os argumentos adicionais
     * e a única variável de ambiente fornecida ao execve.
     */
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

    /* Informa em stderr que os parâmetros de inicialização não conferem. */
    (void)czk_write(
        2,
        startup_fail,
        (unsigned int)sizeof(startup_fail) - 1U
    );

    return 4;
}