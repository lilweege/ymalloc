// internal header
// don't include this file, include "ymalloc.h" instead

#ifndef HEAP_H
#define HEAP_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

typedef size_t BlockSize; // lsb represent freed status
typedef struct BlockNode BlockNode;
struct BlockNode {
    BlockNode* link[2];
    // serves as either a doubly linked list or a binary tree, depending on implementations
    // in an rb tree, node color can be stored in a lsb (cast to int and mask)
    // a doubly linked list is not entirely useful
};

typedef enum {
    BLOCK_USED = 0,
    BLOCK_FREE = 1,
} BlockUsage;

// set
#define BLOCKSIZE_ALLOC(x)        ((x) &= ~1)
#define BLOCKSIZE_FREE(x)         ((x) |= 1)
// get
#define BLOCKSIZE_USAGE(x)        ((x) & 1)
#define BLOCKSIZE_BYTES(x)        ((x) & ~1)


#ifdef DEBUG
    #define HEAP_INIT_SIZE (4096-BLOCK_AUXILIARY_SIZE)
#else
    #define HEAP_INIT_SIZE 1024
#endif
#define SBRK_OK(p) ((p) != (void*) -1)

#define BUGGY_MAX_(a, b) ((a) > (b) ? (a) : (b))
#define HEAP_ALIGNMENT (sizeof(uintptr_t))
#define BLOCK_HEADER_SIZE (sizeof(BlockSize))
#define BLOCK_AUXILIARY_SIZE (BLOCK_HEADER_SIZE*2)
#define PAYLOAD_MIN_SIZE (sizeof(BlockNode))
#define BLOCK_MIN_SIZE (BLOCK_AUXILIARY_SIZE + PAYLOAD_MIN_SIZE)

#define HEAP_ALIGN_UP(sz) (((sz) + (HEAP_ALIGNMENT-1)) & ~(HEAP_ALIGNMENT-1))
#define PAYLOAD_ALIGN(sz) HEAP_ALIGN_UP(BUGGY_MAX_(sz, PAYLOAD_MIN_SIZE))
// block size must be at least the size of a BlockNode (two pointers)
// the header and footer BlockSize implicit list nodes are not included in the block

// on a 64 bit machine, each block should have the following layout
// |   header (8)   |   payload (8*k + 16)   |   footer (8)   |
// freed nodes reuse the payload space to store pointers, so it must be >= 16 bytes
// therefore each successful allocation will reserve at least 32 bytes on the heap

void* HeapBegin(void);
void* HeapEnd(void);
BlockNode* InitBlock(void* ptr, size_t size, BlockUsage use);
void* HeapInit(void);
BlockSize* HeapGrow(size_t size);

#endif // HEAP_H
