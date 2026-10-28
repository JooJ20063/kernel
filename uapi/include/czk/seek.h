#ifndef CZK_UAPI_SEEK_H
#define CZK_UAPI_SEEK_H

/*
 * Referências de posicionamento utilizadas pela syscall lseek().
 * O deslocamento informado é somado à referência selecionada.
 *
 * Estes valores são alternativas, não flags combináveis.
 * Seus valores numéricos fazem parte da ABI compartilhada com o userspace.
 */

/* Calcula a nova posição a partir do início do arquivo. */
#define CZK_SEEK_SET 0U

/* Calcula a nova posição a partir do cursor atual do descritor. */
#define CZK_SEEK_CUR 1U

/* Calcula a nova posição a partir do tamanho atual do arquivo. */
#define CZK_SEEK_END 2U

#endif /* CZK_UAPI_SEEK_H */