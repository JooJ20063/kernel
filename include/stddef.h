#ifndef STDDEF_H
#define STDDEF_H

/* Tipo inteiro não assinado usado para representar tamanhos de objetos e buffers de memória */
typedef unsigned long size_t;

/* Tipo inteiro assinado que armazena a diferença resultante da subtração entre dois ponteiros */
typedef signed long ptrdiff_t;

/* Valor constante que representa um ponteiro inválido ou nulo */
#define NULL ((void *)0)

/**
 * Retorna o deslocamento em bytes (offset) de um campo dentro de uma estrutura.
 * Faz o cast de um ponteiro nulo (0) para o tipo da estrutura e pega o endereço do membro especificado.
 */
#define offsetof(type, member) ((size_t) &((type *)0)->member)

#endif