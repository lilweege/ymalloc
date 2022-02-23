#include "ymalloc.h"

static BlockSize* freeListHead = NULL;

#ifndef DEBUG
#define DumpHeap(x) do{} while(0)
#else
#include <stdio.h>
static void DumpHeap(FILE* stream) {
    fprintf(stream, "====== DUMPING HEAP ======\n");
    BlockNode* curr = (BlockNode*) (((uint8_t*) freeListHead) + BLOCK_HEADER_SIZE);
    while (curr) {
        BlockSize* header = (BlockSize*) (((uint8_t*) curr) - BLOCK_HEADER_SIZE);
        size_t blockSize = BLOCKSIZE_BYTES(*header);
        size_t blockSizeFoot = BLOCKSIZE_BYTES(*(BlockSize*) (((uint8_t*) curr) + blockSize));
        BlockUsage freed = BLOCKSIZE_USAGE(*header);
        fprintf(stream, "%p: { freed=%d, ", (void*)header, freed);
        if (freed == BLOCK_FREE) {
            fprintf(stream, "( next = %p, prev = %p ), ",
                    (void*)curr->next, (void*)curr->prev);
        }
        fprintf(stream, "head=%zu, foot=%zu }\n", blockSize, blockSizeFoot);
        curr = curr->next;
    }
    fprintf(stream, "====== DONE DUMPING ======\n");
}
#endif

// finds the "best fit" for a block with payload size "size"
// without foresight, minimize fragmentation by choosing the closest fit
static BlockSize* BestFit(size_t size) {
    // init heap if not already
    if (!freeListHead)
        freeListHead = HeapInit();
    
    BlockSize* bestBlock = NULL;
    size_t leastWaste = SIZE_MAX;

    BlockNode* curr = (BlockNode*) (((uint8_t*) freeListHead) + BLOCK_HEADER_SIZE);
    while (curr) {
        BlockSize* header = (BlockSize*) (((uint8_t*) curr) - BLOCK_HEADER_SIZE);
        size_t blockSize = BLOCKSIZE_BYTES(*header);

        // can use entire block if possible
        if (blockSize == size)
            return header;
        
        // save the best fit that would split a block
        size_t sizeNeeded = size + BLOCK_MIN_SIZE;
        if (blockSize >= sizeNeeded)
            if (leastWaste > blockSize - sizeNeeded) {
                leastWaste = blockSize - sizeNeeded;
                bestBlock = header;
            }
        
        // next blocksize
        curr = curr->next;
    }

    return bestBlock;
}

static BlockSize* SplitBlock(BlockSize* block, size_t size) {
    // | header (8) | next (8), prev (8)                                            | footer (8) |
    //   ^ block (in free list) points here
    // | header (8) | payload (size) | footer (8) | header (8) | next (8), prev (8) | footer (8) |
    //   ^ return this block                        ^ add this block to free list
    
    BlockNode* oldNode = (BlockNode*) (((uint8_t*) block) + BLOCK_HEADER_SIZE);
    BlockNode* prev = oldNode->prev;
    BlockNode* next = oldNode->next;
    size_t oldSize = BLOCKSIZE_BYTES(*block);
    size_t newSize = oldSize-size-BLOCK_AUXILIARY_SIZE;

    // initialize used block
    InitBlock(block, size, BLOCK_USED);

    // initialize free block
    BlockSize* shrunk = (BlockSize*) (((uint8_t*) block) + BLOCK_AUXILIARY_SIZE + size);
    BlockNode* newNode = InitBlock(shrunk, newSize, BLOCK_FREE);

    // reassign pointers to block
    if (prev)
        prev->next = newNode;
    else
        freeListHead = shrunk;
    if (next)
        next->prev = newNode;
    
    // assign shrunk block pointers
    newNode->prev = prev;
    newNode->next = next;

    DumpHeap(stderr);
    return block;
}

void* ymalloc(size_t size) {
    // nothing to allocate
    if (size == 0)
        return NULL;
    size = PAYLOAD_ALIGN(size);
    
    // find an appropriate block
    BlockSize* chosenBlock = BestFit(size);
    if (!chosenBlock) {
        // TODO: grow the heap
        return NULL;
    }
    
    // split block
    BlockSize* header = SplitBlock(chosenBlock, size);
    if (!header) // probably unreachable
        return NULL;
    
    // initialize block and return payload
    BlockNode* payload = InitBlock(header, size, BLOCK_USED);
    return payload;
}

void* yrealloc(void* ptr, size_t size) {
    // realloc nothing, simply malloc
    if (ptr == NULL)
        return ymalloc(size);

    // TODO ...
    return NULL;
}

void yfree(void* ptr) {
    // nothing to free
    if (ptr == NULL)
        return;

    
}
