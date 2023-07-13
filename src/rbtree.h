#ifndef RBTREE_H
#define RBTREE_H

#include "heap.h"
#include <stdint.h>

typedef enum {
    RB_BLACK = 0,
    RB_RED = 1,
} NodeColor;

#define RB_NODE_LEFT(x) ((BlockNode*)(((uintptr_t)(x)->link[0]) & ~1))
#define RB_NODE_SET_LEFT(x, l) ((x)->link[0] = (BlockNode*)(((uintptr_t)l) | RB_NODE_COLOR(x)))
#define RB_NODE_RIGHT(x) ((x)->link[1])
#define RB_NODE_SET_RIGHT(x, r) ((x)->link[1] = r)
#define RB_NODE_COLOR(x) (((uintptr_t)(x)->link[0]) & 1)
#define RB_NODE_SET_COLOR(x, col) (((x)->link[0]) = (BlockNode*)(((uintptr_t)(x)->link[0] & ~1) | col))
#define RB_NODE_SET_COLOR_RED(x) (((x)->link[0]) = (BlockNode*)(((uintptr_t)(x)->link[0] | 1)))
#define RB_NODE_SET_COLOR_BLACK(x) (((x)->link[0]) = (BlockNode*)(((uintptr_t)(x)->link[0] & ~1)))
#define RB_NODE_FLIP_COLOR(x) (((x)->link[0]) = (BlockNode*)(((uintptr_t)(x)->link[0] ^ 1)))
#define RB_NODE_KEY(x) (RB_Key)(BLOCKSIZE_BYTES(*(BlockSize*)(((uint8_t*)(x)) - BLOCK_HEADER_SIZE)) >> 1)

typedef int64_t RB_Key;

BlockNode* RB_Ceiling(BlockNode* root, size_t size);
void RB_Delete(BlockNode** root, BlockNode* toDelete);
void RB_Put(BlockNode** root, BlockNode* toInsert);
void RB_AssertInvariants(BlockNode* root);


#endif // RBTREE_H
