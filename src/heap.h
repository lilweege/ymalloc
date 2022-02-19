// internal header
// don't include this file, include "ymalloc.h" instead

#ifndef HEAP_H
#define HEAP_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#define HEAP_INIT_SIZE 1024
#define HEAP_ALIGN sizeof(uintptr_t)
#define ALIGN_UP(x, align) ((x) + (align-(((x)-1) & (align-1))-1))
#define SBRK_OK(p) ((p) != (void*) -1)


static_assert(HEAP_ALIGN == 8,
    "sizeof ptr must be 8 bytes!");

typedef struct Block Block;
struct Block {
    Block *prev, *next;
    intptr_t size;
};

bool HeapInit();
bool HeapGrow();

#endif // HEAP_H
