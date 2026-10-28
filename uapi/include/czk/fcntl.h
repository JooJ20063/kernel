#ifndef CZK_UAPI_FCNTL_H
#define CZK_UAPI_FCNTL_H

/*
 * Flags públicas de abertura de arquivos utilizadas pela syscall open().
 * Os valores numéricos fazem parte da ABI compartilhada com o userspace.
 *
 * O modo de acesso ocupa os dois bits inferiores. Deve-se escolher
 * exatamente um modo: RDONLY, WRONLY ou RDWR, combinando-o com flags
 * adicionais por meio do operador OR bit a bit (|).
 */

/* Abre somente para leitura. Valor zero: não deve ser testado com flags & RDONLY. */
#define CZK_O_RDONLY  0x0000U

/* Abre somente para escrita. */
#define CZK_O_WRONLY  0x0001U

/* Abre para leitura e escrita. */
#define CZK_O_RDWR    0x0002U

/*
 * Máscara para extrair o modo: flags & CZK_O_ACCMODE.
 * Não é um modo de abertura; o resultado 0x0003 é inválido.
 */
#define CZK_O_ACCMODE 0x0003U

/* Solicita a criação do arquivo caso ele ainda não exista. */
#define CZK_O_CREAT   0x0040U

#endif /* CZK_UAPI_FCNTL_H */