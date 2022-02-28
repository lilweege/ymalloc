#include "ymalloc.h"
#include <string.h>

static bool didInitHeap = false;
static BlockSize* freeListHead = NULL;

#include <stdio.h>
void DumpFreeList(void) {
    fprintf(stderr, "====== DUMPING HEAP ======\n");
    if (freeListHead) {
        BlockNode* curr = (BlockNode*) (((uint8_t*) freeListHead) + BLOCK_HEADER_SIZE);
        while (curr) {
            BlockSize* header = (BlockSize*) (((uint8_t*) curr) - BLOCK_HEADER_SIZE);
            size_t blockSize = BLOCKSIZE_BYTES(*header);
            size_t blockSizeFoot = BLOCKSIZE_BYTES(*(BlockSize*) (((uint8_t*) curr) + blockSize));
            BlockUsage freed = BLOCKSIZE_USAGE(*header);
            fprintf(stderr, "%p: { freed=%d, ", (void*)header, freed);
            if (freed == BLOCK_FREE) {
                fprintf(stderr, "( next = %p, prev = %p ), ",
                        (void*)(curr->next), (void*)(curr->prev));
            }
            fprintf(stderr, "head=%zu, foot=%zu }\n", blockSize, blockSizeFoot);
            curr = curr->next;
        }
    }
    fprintf(stderr, "====== DONE DUMPING ======\n");
}

// splits a free block and repairs the remaining block links
// returns the newly shrunk free block
// NOTE: assumes block is big enough to accomodate the smallest new block
// NOTE: does not initialize the newly split block header/footer
static BlockSize* SplitBlock(BlockSize* block, size_t size) {
    // | header (8) | next (8), prev (8)                                            | footer (8) |
    //   ^ block (in free list) points here
    // | header (8) | payload (size) | footer (8) | header (8) | next (8), prev (8) | footer (8) |
    //                                              ^ add this block to free list
    BlockNode* oldNode = (BlockNode*) (((uint8_t*) block) + BLOCK_HEADER_SIZE);
    BlockNode* prev = oldNode->prev;
    BlockNode* next = oldNode->next;
    size_t oldSize = BLOCKSIZE_BYTES(*block);
    assert(size + BLOCK_MIN_SIZE <= oldSize);
    // assert(size + BLOCK_MIN_SIZE <= oldSize + BLOCK_AUXILIARY_SIZE); // realloc
    size_t newSize = oldSize - size - BLOCK_AUXILIARY_SIZE;

    // initialize free block
    BlockSize* shrunk = (BlockSize*) (((uint8_t*) block) + BLOCK_AUXILIARY_SIZE + size);
    BlockNode* newNode = InitBlock(shrunk, newSize, BLOCK_FREE);
    // BROKEN FOR REALLOC

    if (BLOCKSIZE_USAGE(*block) == BLOCK_FREE) {
        // reassign pointers to block
        if (prev)
            prev->next = newNode;
        else {
            if (freeListHead != block) {
                printf("FREE HEAD = %p\n", (void*) freeListHead);
            }
            // assert(freeListHead == block);
            freeListHead = shrunk;
        }
        if (next)
            next->prev = newNode;
    }
    
    // assign shrunk block pointers
    newNode->prev = prev;
    newNode->next = next;
    return shrunk;
}

// finds the "best fit" for a block with payload size "size"
// without foresight, minimize fragmentation by choosing the closest fit
static BlockSize* BestFit(size_t size) {
    if (!freeListHead)
        return NULL;
    
    BlockSize* bestBlock = NULL;
    size_t leastWaste = SIZE_MAX;

    BlockNode* curr = (BlockNode*) (((uint8_t*) freeListHead) + BLOCK_HEADER_SIZE);
    while (curr) {
        BlockSize* header = (BlockSize*) (((uint8_t*) curr) - BLOCK_HEADER_SIZE);
        size_t blockSize = BLOCKSIZE_BYTES(*header);

        // can use entire block if possible
        if (blockSize == size) {
            // no need to split
            // remove entire block from free list and return it
            BlockNode* node = (BlockNode*) (((uint8_t*) header) + BLOCK_HEADER_SIZE);
            BlockNode* prev = node->prev;
            BlockNode* next = node->next;
            if (prev)
                prev->next = next;
            else {
                assert(freeListHead == header);
                if (next)
                    freeListHead = (BlockSize*) (((uint8_t*) next) - BLOCK_HEADER_SIZE);
                else
                    freeListHead = NULL;
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

    // couldn't find a block (will have to grow heap)
    if (!bestBlock)
        return NULL;

    // split the block, rejoin the list, and return the newly created block
    SplitBlock(bestBlock, size);
    return bestBlock;
}


// try to join a newly freed block with any adjacent free block
// NOTE: this step happens before the block is added to the free list
// NOTE: this does not set the block pointers
static BlockSize* CoalesceBlocks(BlockSize* block) {
    size_t blockSize = BLOCKSIZE_BYTES(*block);

    BlockSize* aboveFooter = (BlockSize*) (((uint8_t*) block) - BLOCK_HEADER_SIZE);
    BlockSize* belowHeader = (BlockSize*) (((uint8_t*) block) + BLOCK_AUXILIARY_SIZE + blockSize);

    // merge block with above and below blocks if they are free
    if (((void*) aboveFooter > HeapBegin()) &&
            BLOCKSIZE_USAGE(*aboveFooter) == BLOCK_FREE) {
        printf("MERGING ABOVE\n");
        size_t aboveSize = BLOCKSIZE_BYTES(*aboveFooter);
        BlockNode* aboveNode = (BlockNode*) (((uint8_t*) aboveFooter) - aboveSize);
        BlockNode* prev = aboveNode->prev;
        BlockNode* next = aboveNode->next;

        // join blocks
        printf("MERGING ABOVE %p WITH %p\n", (void*) (((uint8_t*) aboveNode) - BLOCK_HEADER_SIZE), (void*) block);
        block = (BlockSize*) (((uint8_t*) aboveNode) - BLOCK_HEADER_SIZE);
        blockSize += aboveSize + BLOCK_AUXILIARY_SIZE;

        // remove aboveNode from list
        if (prev)
            prev->next = next;
        else {
            assert(freeListHead == block);
            if (next)
                freeListHead = (BlockSize*) (((uint8_t*) next) - BLOCK_HEADER_SIZE);
            else
                freeListHead = NULL;
        }
        if (next)
            next->prev = prev;
    }
    if (((void*) belowHeader < HeapEnd()) &&
            BLOCKSIZE_USAGE(*belowHeader) == BLOCK_FREE) {
        printf("MERGING BELOW\n");
        size_t belowSize = BLOCKSIZE_BYTES(*belowHeader);
        BlockNode* belowNode = (BlockNode*) (((uint8_t*) belowHeader) + BLOCK_HEADER_SIZE);
        BlockNode* prev = belowNode->prev;
        BlockNode* next = belowNode->next;

        // join blocks
        blockSize += belowSize + BLOCK_AUXILIARY_SIZE;

        if (prev)
            prev->next = next;
        else {
            assert(freeListHead == (BlockSize*) (((uint8_t*) belowNode) - BLOCK_HEADER_SIZE));
            if (next)
                freeListHead = (BlockSize*) (((uint8_t*) next) - BLOCK_HEADER_SIZE);
            else
                freeListHead = NULL;
        }
        if (next)
            next->prev = prev;
    }

    printf("BLOCK SIZE = %zu\n", blockSize);
    printf("block = %p\n", (void*) block);
    InitBlock(block, blockSize, BLOCK_FREE);
    return block;
}

static void PrependFreeBlock(BlockSize* block) {
    // set the block pointers (insert at front of the free list)
    BlockNode* node = (BlockNode*) (((uint8_t*) block) + BLOCK_HEADER_SIZE);
    if (freeListHead) {
        node->next = (BlockNode*) (((uint8_t*) freeListHead) + BLOCK_HEADER_SIZE);
        if (node->next) {
            printf("node->next = %p\n", (void*) node->next);
            printf("node->next->prev = %p\n", (void*) node->next->prev);
            assert(node->next->prev == NULL);
            node->next->prev = node;
        }
    }
    else {
        node->next = NULL;
    }
    node->prev = NULL;
    freeListHead = block;
}

void* ymalloc(size_t size) {
    // nothing to allocate
    if (size == 0)
        return NULL;
    size = PAYLOAD_ALIGN(size);
    
    if (!didInitHeap) {
        didInitHeap = true;
        freeListHead = HeapInit();
    }
    
    printf("ALIGNED SIZE = %zu\n", size);
    // find an appropriate block
    BlockSize* block = BestFit(size);
    if (!block) {
        printf("GROWING HEAP!\n");
        block = HeapGrow(size);
        // printf("freeListHead = %p\n", (void*)freeListHead);
        BlockSize* coalesced = CoalesceBlocks(block);
        // printf("freeListHead = %p\n", (void*)freeListHead);

        // if the newly grown block coalesced with above, split it
        if (block != coalesced) {
            block = coalesced;
            printf("block = %p\n", (void*)block);
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
    PrependFreeBlock(CoalesceBlocks(block));
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
    printf("OLD SIZE = %zu\n", oldSize);
    printf("ALIGNED SIZE = %zu\n", size);

    // same size, do nothing
    if (size + BLOCK_MIN_SIZE > oldSize && size <= oldSize)
        return ptr;
    
    // lower size, shrink block
    if (size < oldSize) {
        printf("SHRINKING BLOCK\n");
        BlockSize* removed = SplitBlock(block, size);
        InitBlock(block, size, BLOCK_USED);
        PrependFreeBlock(CoalesceBlocks(removed));
        return ptr;
    }
    
    // below here size > oldSize
    BlockSize* belowHeader = (BlockSize*) (((uint8_t*) ptr) + oldSize + BLOCK_HEADER_SIZE);
    
    // check if it is possible to grow without reallocating
    // check if block immediately below is free and big enough
    if (((void*) belowHeader < HeapEnd()) &&
            BLOCKSIZE_USAGE(*belowHeader) == BLOCK_FREE) {
        size_t belowSize = BLOCKSIZE_BYTES(*belowHeader);
        printf("REALLOC BELOW FREE\n");

        // is merged size big enough
        size_t exactSize = oldSize + belowSize + BLOCK_AUXILIARY_SIZE;
        printf("size = %zu\n", size);
        printf("exactSize = %zu\n", exactSize);
        if (size == exactSize) {
            // remove from free list (no need to split)
            printf("REALLOC EXACT SIZE!\n");
            BlockNode* node = (BlockNode*) (((uint8_t*) belowHeader) + BLOCK_HEADER_SIZE);
            BlockNode* prev = node->prev;
            BlockNode* next = node->next;
            if (prev)
                prev->next = next;
            else {
                // assert(0);
                if (next)
                    freeListHead = (BlockSize*) (((uint8_t*) next) - BLOCK_HEADER_SIZE);
                else
                    freeListHead = NULL;
            }
            if (next)
                next->prev = prev;

            InitBlock(block, size, BLOCK_USED);
            return ptr;
        }
        if (size + BLOCK_MIN_SIZE < exactSize) {
            // remove from free list (split)
            printf("SPLITTING size = %zu\n", size);
            printf("SPLITTING oldSize = %zu\n", oldSize);
            size_t splitSize = size - oldSize - BLOCK_HEADER_SIZE;
            printf("SPLITTING splitSize = %zu\n", splitSize);
            if (size < oldSize + BLOCK_HEADER_SIZE) {
                assert(0);
            }

            SplitBlock(belowHeader, splitSize);
            InitBlock(block, size, BLOCK_USED);
            return ptr;
        }
    }

    // need to reallocate and move

    // TODO
    // coalesce and move

    return NULL;
}
