#include "ymalloc.h"
#include "heap.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>


#ifdef DEBUG
    #define dbgf(fmt, ...) printf(fmt, ##__VA_ARGS__)
    // #define dbgf(...)
#else
    #define dbgf(...)
#endif



static bool didInitHeap = false;
#define LL_IMPL 0


#if LL_IMPL

#define LL_FIRST_FIT 0

static BlockNode* freeHead = NULL;

// inserts a free block to the free list/tree
static void InsertFreeBlock(BlockSize* block) {
    assert(block != NULL);
    assert(BLOCKSIZE_USAGE(*block) == BLOCK_FREE);

    // set the block pointers (insert at front of the free list)
    BlockNode* node = (BlockNode*) (((uint8_t*) block) + BLOCK_HEADER_SIZE);

#ifdef DEBUG
    for (BlockNode* curr = freeHead;
        curr != NULL;
        curr = curr->link[1])
    {
        assert(curr != node);
    }
#endif

    if (freeHead) {
        node->link[1] = freeHead;
        if (node->link[1]) {
            dbgf("node->next = %p\n", (void*) node->link[1]);
            dbgf("node->next->prev = %p\n", (void*) node->link[1]->link[0]);
            assert(node->link[1]->link[0] == NULL);
            node->link[1]->link[0] = node;
        }
    }
    else {
        node->link[1] = NULL;
    }
    node->link[0] = NULL;
    freeHead = node;
}

// removes a free block from the free list/tree
static void RemoveFreeBlock(BlockSize* block) {
    assert(block != NULL);
    assert(BLOCKSIZE_USAGE(*block) == BLOCK_FREE);

    BlockNode* node = (BlockNode*) (((uint8_t*) block) + BLOCK_HEADER_SIZE);

#ifdef DEBUG
    bool hasNode = false;
    for (BlockNode* curr = freeHead;
        curr != NULL;
        curr = curr->link[1])
    {
        if (curr == node) {
            hasNode = true;
            break;
        }
    }
    assert(hasNode);
#endif

    BlockNode* prev = node->link[0];
    BlockNode* next = node->link[1];
    if (prev)
        prev->link[1] = next;
    else {
        if (next)
            freeHead = next;
        else
            freeHead = NULL;
    }
    if (next)
        next->link[0] = prev;
}

// returns the smallest free block larger than size, such that either
// 1. the block has the exact correct size (no split)
// 2. the block is larger than size + BLOCK_MIN_SIZE (split)
static BlockSize* BestFreeBlock(size_t size) {
    if (!freeHead)
        return NULL;

    size_t sizeNeeded = size + BLOCK_MIN_SIZE;
    BlockSize* bestBlock = NULL;
    size_t leastWaste = SIZE_MAX;
    for (BlockNode* curr = freeHead;
        curr != NULL;
        curr = curr->link[1])
    {
        BlockSize* header = (BlockSize*) (((uint8_t*) curr) - BLOCK_HEADER_SIZE);
        size_t blockSize = BLOCKSIZE_BYTES(*header);

        // exact fit
        if (blockSize == size) {
            return header;
        }

        // save the best fit that would split a block
        if (blockSize >= sizeNeeded) {
#if LL_FIRST_FIT
            return header;
#else
            if (leastWaste > blockSize - sizeNeeded) {
                leastWaste = blockSize - sizeNeeded;
                bestBlock = header;
            }
#endif
        }
    }
    return bestBlock;
}

#else

#include "rbtree.h"

static BlockNode* freeRoot = NULL;

static void RemoveFreeBlock(BlockSize* block) {
    RB_Delete(&freeRoot, (BlockNode*) (((uint8_t*) block) + BLOCK_HEADER_SIZE));
#ifdef DEBUG
    RB_AssertInvariants(freeRoot);
#endif
}

static void InsertFreeBlock(BlockSize* block) {
    assert(block != NULL);
    assert(BLOCKSIZE_USAGE(*block) == BLOCK_FREE);
    BlockNode* node = (BlockNode*) (((uint8_t*) block) + BLOCK_HEADER_SIZE);
    RB_NODE_SET_LEFT(node, NULL);
    RB_NODE_SET_RIGHT(node, NULL);
    RB_Put(&freeRoot, node);
#ifdef DEBUG
    RB_AssertInvariants(freeRoot);
#endif
}

static BlockSize* BestFreeBlock(size_t size) {
    size_t sizeNeeded = size + BLOCK_MIN_SIZE;
    BlockNode* bestNode = RB_Ceiling(freeRoot, sizeNeeded);
    if (bestNode == NULL)
        return NULL;
    BlockSize* best = (BlockSize*) (((uint8_t*) bestNode) - BLOCK_HEADER_SIZE);
    assert(*best >= sizeNeeded);
#ifdef DEBUG
    RB_AssertInvariants(freeRoot);
#endif
    return best;
}


#endif

/**/

// splits a free block and repairs the remaining block links
// returns the newly shrunk free block
// NOTE: assumes block is big enough to accomodate the smallest new block
// NOTE: does not initialize the newly split block header/footer
static BlockSize* SplitBlock(BlockSize* block, size_t size) {
    // | header (8) | next (8), prev (8)                                                     | footer (8) |
    //   ^ block points here
    // | header (8) | payload (size) | footer (8) | header (8) | next (8), prev (8)   ...    | footer (8) |
    //                                              ^ add this block to free list

    // block will always be free unless call came from realloc
    if (BLOCKSIZE_USAGE(*block) == BLOCK_FREE) {
        RemoveFreeBlock(block);
    }

    BlockNode* oldNode = (BlockNode*) (((uint8_t*) block) + BLOCK_HEADER_SIZE);
    size_t oldSize = BLOCKSIZE_BYTES(*block);
    assert(size + BLOCK_MIN_SIZE <= oldSize);
    // assert(size + BLOCK_MIN_SIZE <= oldSize + BLOCK_AUXILIARY_SIZE); // realloc
    size_t newSize = oldSize - size - BLOCK_AUXILIARY_SIZE;

    // initialize free block
    BlockSize* shrunk = (BlockSize*) (((uint8_t*) block) + BLOCK_AUXILIARY_SIZE + size);
    InitBlock(shrunk, newSize, BLOCK_FREE);
    assert(oldNode == InitBlock(block, size, BLOCKSIZE_USAGE(*block)));

    if (BLOCKSIZE_USAGE(*block) == BLOCK_FREE) {
        InsertFreeBlock(shrunk);
    }
    return shrunk;
}


// finds the "best fit" for a block with payload size "size"
// without foresight, minimize fragmentation by choosing the closest fit
static BlockSize* BestFit(size_t size) {
    BlockSize* bestBlock = BestFreeBlock(size);

    // couldn't find a block (will have to grow heap)
    if (!bestBlock)
        return NULL;

    // exact match, no need to split
    size_t blockSize = BLOCKSIZE_BYTES(*bestBlock);
    if (blockSize == size) {
        // remove entire block from free list and return it
        RemoveFreeBlock(bestBlock);
        return bestBlock;
    }

    // split the block, rejoin the list, and return the newly created block
    // assertion will fail if block is not large enough to split by size
    SplitBlock(bestBlock, size);
    return bestBlock;
}


// try to join a newly freed block with any adjacent free block
// NOTE: this step happens before the block is added to the free list
// NOTE: this does not set the block pointers
static BlockSize* CoalesceBlocks(BlockSize* block) {
    dbgf("COALESCING\n");
    size_t blockSize = BLOCKSIZE_BYTES(*block);
    dbgf("BLOCK SIZE = %zu\n", blockSize);
    dbgf("block = %p\n", (void*) block);
    if (BLOCKSIZE_USAGE(*block) == BLOCK_FREE) {
        RemoveFreeBlock(block);
    }

    BlockSize* aboveFooter = (BlockSize*) (((uint8_t*) block) - BLOCK_HEADER_SIZE);
    BlockSize* belowHeader = (BlockSize*) (((uint8_t*) block) + BLOCK_AUXILIARY_SIZE + blockSize);

    // merge block with above and below blocks if they are free
    if ((void*) aboveFooter > HeapBegin() &&
        BLOCKSIZE_USAGE(*aboveFooter) == BLOCK_FREE)
    {
        dbgf("MERGING ABOVE\n");
        size_t aboveSize = BLOCKSIZE_BYTES(*aboveFooter);
        BlockNode* aboveNode = (BlockNode*) (((uint8_t*) aboveFooter) - aboveSize);

        // join blocks
        dbgf("MERGING ABOVE %p WITH %p\n", (void*) (((uint8_t*) aboveNode) - BLOCK_HEADER_SIZE), (void*) block);
        block = (BlockSize*) (((uint8_t*) aboveNode) - BLOCK_HEADER_SIZE);
        blockSize += aboveSize + BLOCK_AUXILIARY_SIZE;

        // remove aboveNode from list
        RemoveFreeBlock(block);
    }
    if ((void*) belowHeader < HeapEnd() &&
        BLOCKSIZE_USAGE(*belowHeader) == BLOCK_FREE)
    {
        dbgf("MERGING BELOW\n");
        size_t belowSize = BLOCKSIZE_BYTES(*belowHeader);

        // join blocks
        blockSize += belowSize + BLOCK_AUXILIARY_SIZE;

        RemoveFreeBlock(belowHeader);
    }

    InitBlock(block, blockSize, BLOCK_FREE);
    return block;
}


void* ymalloc(size_t size) {
    // nothing to allocate
    if (size == 0)
        return NULL;
    size = PAYLOAD_ALIGN(size);

    if (!didInitHeap) {
        didInitHeap = true;
        InsertFreeBlock(HeapInit());
    }

    dbgf("ALIGNED SIZE = %zu\n", size);
    // find an appropriate block
    BlockSize* block = BestFit(size);
    if (!block) {
        dbgf("GROWING HEAP!\n");
        block = HeapGrow(size);
        assert(BLOCKSIZE_USAGE(*block) == BLOCK_FREE);

        size_t sizeBeforeCoalescing = BLOCKSIZE_BYTES(*block);
        InsertFreeBlock(block);
        BlockSize* coalesced = CoalesceBlocks(block);
        size_t sizeAfterCoalescing = BLOCKSIZE_BYTES(*coalesced);

        // if the newly grown block coalesced with above, split it
        if (block != coalesced) {
            block = coalesced;
            assert(sizeBeforeCoalescing != sizeAfterCoalescing);
        }
        if (sizeBeforeCoalescing != sizeAfterCoalescing) {
            dbgf("block = %p\n", (void*)block);
            InsertFreeBlock(coalesced);
            SplitBlock(block, size);
        }
    }

    // initialize block and return payload
    BlockNode* payload = InitBlock(block, size, BLOCK_USED);
    return payload;
}

void yfree(void* ptr) {
    // nothing to free
    if (ptr == NULL)
        return;

    // coalesce adjacent blocks
    BlockSize* block = (BlockSize*) (((uint8_t*) ptr) - BLOCK_HEADER_SIZE);
    InsertFreeBlock(CoalesceBlocks(block));
}

void* ycalloc(size_t nmemb, size_t size) {
    // NOTE: nmemb * size can overflow!
    size_t totSize = nmemb * size;
    void* ptr = ymalloc(totSize);
    if (ptr)
        memset(ptr, 0, totSize);
    return ptr;
}

void* yrealloc(void* ptr, size_t size) {
    // realloc nothing, simply malloc
    if (ptr == NULL)
        return ymalloc(size);

    // get the old and new sizes
    BlockSize* block = (BlockSize*) (((uint8_t*) ptr) - BLOCK_HEADER_SIZE);
    size_t oldSize = BLOCKSIZE_BYTES(*block);
    size = PAYLOAD_ALIGN(size);
    dbgf("OLD SIZE = %zu\n", oldSize);
    dbgf("ALIGNED SIZE = %zu\n", size);

    // same size, do nothing
    if (size + BLOCK_MIN_SIZE > oldSize && size <= oldSize)
        return ptr;

    // lower size, shrink block
    if (size < oldSize) {
        dbgf("SHRINKING BLOCK\n");
        BlockSize* removed = SplitBlock(block, size);
        InitBlock(block, size, BLOCK_USED);
        InsertFreeBlock(CoalesceBlocks(removed));
        return ptr;
    }

    // below here size > oldSize
    BlockSize* belowHeader = (BlockSize*) (((uint8_t*) ptr) + oldSize + BLOCK_HEADER_SIZE);

    // check if it is possible to grow without reallocating
    // check if block immediately below is free and big enough
    if ((void*) belowHeader < HeapEnd() &&
        BLOCKSIZE_USAGE(*belowHeader) == BLOCK_FREE)
    {
        size_t belowSize = BLOCKSIZE_BYTES(*belowHeader);
        dbgf("REALLOC BELOW FREE\n");

        // is merged size big enough
        size_t exactSize = oldSize + belowSize + BLOCK_AUXILIARY_SIZE;
        dbgf("size = %zu\n", size);
        dbgf("exactSize = %zu\n", exactSize);
        if (size == exactSize) {
            // remove from free list (no need to split)
            dbgf("REALLOC EXACT SIZE!\n");
            RemoveFreeBlock(belowHeader);

            InitBlock(block, size, BLOCK_USED);
            return ptr;
        }

        if (size + BLOCK_MIN_SIZE <= exactSize) {
            // remove from free list (split)
            size_t splitSize = size - oldSize;
            assert(size >= oldSize);
            dbgf("SPLITTING size = %zu\n", size);
            dbgf("SPLITTING oldSize = %zu\n", oldSize);
            dbgf("SPLITTING belowSize = %zu\n", belowSize);
            dbgf("SPLITTING splitSize = %zu\n", splitSize);

            RemoveFreeBlock(belowHeader);

            // similar to SplitBlock, but don't preserve header space
            size_t newSize = belowSize - splitSize;
            dbgf("SPLITTING newSize = %zu\n", newSize);
            BlockSize* shrunk = (BlockSize*) (((uint8_t*) belowHeader) + splitSize);
            InitBlock(shrunk, newSize, BLOCK_FREE);
            InitBlock(block, size, BLOCK_USED);

            InsertFreeBlock(shrunk);
            return ptr;
        }
        // below block too small
    }

    // TODO: if the above block is empty, check if
    // aboveSize + oldSize + belowSize <= newSize, then coalesce and memmove

    // if the old block cannot be reused in any way, need to reallocate and move
    void* new_ptr = ymalloc(size);
    memcpy(new_ptr, ptr, oldSize);
    yfree(ptr);
    return new_ptr;
}
