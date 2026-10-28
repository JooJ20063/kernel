#ifndef CZK_UAPI_ERRNO_H
#define CZK_UAPI_ERRNO_H

/*
 * Códigos públicos de erro da ABI de syscalls do Cruzeiro.
 *
 * Em caso de falha, as syscalls retornam -CZK_E* em EAX.
 * Zero e valores positivos mantêm o significado de sucesso definido
 * por cada chamada.
 *
 * Os valores numéricos fazem parte da ABI compartilhada com o userspace
 * e não devem ser renumerados sem uma mudança explícita desse contrato.
 */

/* Arquivo ou entrada de diretório inexistente. */
#define CZK_ENOENT       2

/* Argumentos ou ambiente excedem os limites aceitos pela chamada. */
#define CZK_E2BIG        7

/* Formato de executável inválido ou não suportado. */
#define CZK_ENOEXEC      8

/* Descritor inválido ou incompatível com a operação solicitada. */
#define CZK_EBADF        9

/* Não há processo filho elegível para a operação de espera. */
#define CZK_ECHILD      10

/* Memória insuficiente para concluir a operação. */
#define CZK_ENOMEM      12

/* Acesso negado. */
#define CZK_EACCES      13

/* Endereço de memória do usuário inválido ou sem a permissão necessária. */
#define CZK_EFAULT      14

/* Um componente do caminho que deveria ser diretório não é um diretório. */
#define CZK_ENOTDIR     20

/* Argumento ou combinação de argumentos inválida. */
#define CZK_EINVAL      22

/* Limite de descritores abertos do processo atingido. */
#define CZK_EMFILE      24

/* Reposicionamento solicitado em um objeto que não suporta lseek(). */
#define CZK_ESPIPE      29

/* Nome ou caminho excede o comprimento permitido. */
#define CZK_ENAMETOOLONG 36

/* Chamada de sistema não implementada ou número de syscall desconhecido. */
#define CZK_ENOSYS      38

#endif /* CZK_UAPI_ERRNO_H */