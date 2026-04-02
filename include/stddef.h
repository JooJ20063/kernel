#ifndef STDDEF_H
#define STDDEF_H

typedef unsigned long size_t;
typedef signed long ptrdiff_t;
typedef unsigned long uintptr_t;
typedef signed long intptr_t;

#define NULL ((void *)0)
#define offsetof(type, member) ((size_t) &((type *)0)->member)

#endif
