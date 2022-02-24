#include "ymalloc.h"

static BlockSize* freeListHead = NULL;

#include <stdio.h>
static void DumpHeap(FILE* stream) {
    size_t i = 0;
    fprintf(stream, "====== DUMPING HEAP ======\n");
    BlockNode* curr = (BlockNode*) (((uint8_t*) freeListHead) + BLOCK_HEADER_SIZE);
    while (curr) {
        if (++i > 10) {
            fprintf(stream, "INFINITE LOOP, BREAKING\n");
            break;
        }
        BlockSize* header = (BlockSize*) (((uint8_t*) curr) - BLOCK_HEADER_SIZE);
        size_t blockSize = BLOCKSIZE_BYTES(*header);
        size_t blockSizeFoot = BLOCKSIZE_BYTES(*(BlockSize*) (((uint8_t*) curr) + blockSize));
        BlockUsage freed = BLOCKSIZE_USAGE(*header);
        fprintf(stream, "%p: { freed=%d, ", (void*)header, freed);
        if (freed == BLOCK_FREE) {
            fprintf(stream, "( next = %p, prev = %p ), ",
                    (void*)(curr->next), (void*)(curr->prev));
        }
        fprintf(stream, "head=%zu, foot=%zu }\n", blockSize, blockSizeFoot);
        curr = curr->next;
    }
    fprintf(stream, "====== DONE DUMPING ======\n");
}

static BlockSize* SplitBlock(BlockSize* block, size_t size) {
    // | header (8) | next (8), prev (8)                                            | footer (8) |
    //   ^ block (in free list) points here
    // | header (8) | payload (size) | footer (8) | header (8) | next (8), prev (8) | footer (8) |
    //   ^ return this block                        ^ add this block to free list
    printf("block = %p\n", (void*) block);
    BlockNode* oldNode = (BlockNode*) (((uint8_t*) block) + BLOCK_HEADER_SIZE);
    BlockNode* prev = oldNode->prev;
    BlockNode* next = oldNode->next;
    size_t oldSize = BLOCKSIZE_BYTES(*block);
    size_t newSize = oldSize-size-BLOCK_AUXILIARY_SIZE;

    // initialize used block
    // InitBlock(block, size, BLOCK_USED);

    // initialize free block
    BlockSize* shrunk = (BlockSize*) (((uint8_t*) block) + BLOCK_AUXILIARY_SIZE + size);
    BlockNode* newNode = InitBlock(shrunk, newSize, BLOCK_FREE);
    printf("shrunk = %p\n", (void*) shrunk);
    // reassign pointers to block
    if (prev)
        prev->next = newNode;
    else {
        assert(freeListHead == block);
        freeListHead = shrunk;
    }
    if (next)
        next->prev = newNode;
    
    // assign shrunk block pointers
    newNode->prev = prev;
    newNode->next = next;

    // DumpHeap(stderr);
    return block;
}

// finds the "best fit" for a block with payload size "size"
// without foresight, minimize fragmentation by choosing the closest fit
static BlockSize* BestFit(size_t size) {
    // init heap if not already
    if (!freeListHead)
        freeListHead = HeapInit();
    
    printf("BEST FIT WITH SIZE %zu\n", size);
    
    BlockSize* bestBlock = NULL;
    size_t leastWaste = SIZE_MAX;

    BlockNode* curr = (BlockNode*) (((uint8_t*) freeListHead) + BLOCK_HEADER_SIZE);
    while (curr) {
        BlockSize* header = (BlockSize*) (((uint8_t*) curr) - BLOCK_HEADER_SIZE);
        size_t blockSize = BLOCKSIZE_BYTES(*header);

        // can use entire block if possible
        if (blockSize == size) {
            printf("BLOCK EXACT MATCH\n");

            BlockNode* node = (BlockNode*) (((uint8_t*) header) + BLOCK_HEADER_SIZE);
            BlockNode* prev = node->prev;
            BlockNode* next = node->next;
            if (prev)
                prev->next = next;
            else {
                assert(freeListHead == header);
                freeListHead = (BlockSize*) (((uint8_t*) next) - BLOCK_HEADER_SIZE);
            }
            if (next)
                next->prev = prev;

            return header;
        }
        
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

    SplitBlock(bestBlock, size);
    return bestBlock;
}


static BlockSize* CoalesceBlocks(BlockSize* block) {
    // try to join a newly freed block with any adjacent free block
    // NOTE: this step happens before the block is added to the free list
    size_t blockSize = BLOCKSIZE_BYTES(*block);

    BlockSize* newBlock = block;
    BlockSize* aboveFooter = (BlockSize*) (((uint8_t*) block) - BLOCK_HEADER_SIZE);
    BlockSize* belowHeader = (BlockSize*) (((uint8_t*) block) + BLOCK_AUXILIARY_SIZE + blockSize);

    // merge block with blocks above and below if free
    if (((void*) aboveFooter > HeapBegin()) &&
            BLOCKSIZE_USAGE(*aboveFooter) == BLOCK_FREE) {
        // size_t aboveSize = BLOCKSIZE_BYTES(*aboveFooter);
        // BlockNode* aboveNode = (BlockNode*) (((uint8_t*) aboveFooter) - aboveSize);
        // BlockNode* prev = aboveNode->prev;
        // BlockNode* next = aboveNode->next;
        // newBlock = (BlockSize*) (((uint8_t*) aboveNode) - BLOCK_HEADER_SIZE);
        // if (prev)
        //     prev->next = next;
        // else
        //     freeListHead = newBlock;
        // if (next)
        //     next->prev = prev;
        
        // blockSize += aboveSize + BLOCK_AUXILIARY_SIZE;
    }
    if (((void*) belowHeader < HeapEnd()) &&
            BLOCKSIZE_USAGE(*belowHeader) == BLOCK_FREE) {
        // size_t belowSize = BLOCKSIZE_BYTES(*belowHeader);
        // BlockNode* belowNode = (BlockNode*) (((uint8_t*) belowHeader) + BLOCK_HEADER_SIZE);
        // BlockNode* prev = belowNode->prev;
        // BlockNode* next = belowNode->next;
        // if (prev)
        //     prev->next = next;
        // else
        //     freeListHead = newBlock;
        // if (next)
        //     next->prev = prev;
        
        // blockSize += belowSize + BLOCK_AUXILIARY_SIZE;
    }

    InitBlock(newBlock, blockSize, BLOCK_FREE);
    return newBlock;
}

void* ymalloc(size_t size) {
    // nothing to allocate
    if (size == 0)
        return NULL;
    size = PAYLOAD_ALIGN(size);
    
    // find an appropriate block
    BlockSize* block = BestFit(size);
    if (!block) {
        // TODO: grow the heap and coalesce
        return NULL;
    }
    
    // initialize block and return payload
    BlockNode* payload = InitBlock(block, size, BLOCK_USED);
    return payload;
}

void* yrealloc(void* ptr, size_t size) {
    DumpHeap(stderr);
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
    
    // DumpHeap(stderr);

    // coalesce adjacent blocks
    // this does not set the block pointers
    BlockSize* block = (BlockSize*) (((uint8_t*) ptr) - BLOCK_HEADER_SIZE);
    block = CoalesceBlocks(block);

    // set the block pointers (insert at front of the free list)
    BlockNode* node = (BlockNode*) (((uint8_t*) block) + BLOCK_HEADER_SIZE);
    node->next = (BlockNode*) (((uint8_t*) freeListHead) + BLOCK_HEADER_SIZE);
    assert(node->next->prev == NULL);
    node->next->prev = node;
    node->prev = NULL;
    freeListHead = block;
}
