#ifndef _STDINT_H
#define _STDINT_H

/* Tipos inteiros de tamanho fixo exato (ILP32 / LP64) */
typedef signed char     int8_t;
typedef unsigned char   uint8_t;

typedef signed short    int16_t;
typedef unsigned short  uint16_t;

typedef signed int      int32_t;
typedef unsigned int    uint32_t;

typedef signed long long      int64_t;
typedef unsigned long long    uint64_t;

/* 
 * Tipos inteiros com tamanho suficiente para armazenar ponteiros de memória de forma segura.
 * Selecionados via macros de arquitetura definidas pelo compilador (GCC/Clang):
 * - x86_64 (64 bits): usa inteiros de 64 bits
 * - x86 / IA-32 (32 bits): usa inteiros de 32 bits
 */
#if defined(__x86_64__) || defined(__x86_64)
typedef uint64_t    uintptr_t;
typedef int64_t     intptr_t;
#else
typedef uint32_t    uintptr_t;
typedef int32_t     intptr_t;
#endif

#endif