#ifndef CZK_UAPI_SYSCALL_H
#define CZK_UAPI_SYSCALL_H

/*
 * Números públicos das chamadas de sistema do Cruzeiro.
 * Os valores fazem parte da ABI e não devem ser renumerados.
 *
 * Convenção x86 por INT 0x80:
 *   EAX = número da chamada / valor de retorno
 *   EBX = argumento 1
 *   ECX = argumento 2
 *   EDX = argumento 3
 *
 * Falhas retornam códigos negativos definidos em <czk/errno.h>.
 */

/* write(fd, buffer, size): escreve bytes em um descritor. */
#define SYS_WRITE    1

/* exit(status): encerra o processo; não retorna em caso de sucesso. */
#define SYS_EXIT     2

/* getpid(): obtém o identificador do processo atual. */
#define SYS_GETPID   3

/* yield(): cede voluntariamente a CPU ao escalonador. */
#define SYS_YIELD    4

/* getppid(): obtém o identificador do processo pai. */
#define SYS_GETPPID  5

/* sleep(ticks): suspende a execução pelo intervalo solicitado em ticks. */
#define SYS_SLEEP    6

/* wait(status): aguarda um filho e coleta seu PID e status de saída. */
#define SYS_WAIT     7

/* open(path, flags): abre ou cria um arquivo e retorna um descritor. */
#define SYS_OPEN     8

/* read(fd, buffer, size): lê bytes de um descritor. */
#define SYS_READ     9

/* close(fd): fecha um descritor. */
#define SYS_CLOSE   10

/* lseek(fd, offset, whence): reposiciona o cursor de um descritor. */
#define SYS_LSEEK   11

/* fstat(fd, stat): consulta os metadados associados ao descritor. */
#define SYS_FSTAT   12

/* readdir(fd, dirent): obtém a próxima entrada de um diretório aberto. */
#define SYS_READDIR 13

/*
 * execve(path, argv, envp): substitui a imagem do processo atual.
 * Em caso de sucesso, não retorna à imagem anterior.
 */
#define SYS_EXECVE  14

#endif /* CZK_UAPI_SYSCALL_H */