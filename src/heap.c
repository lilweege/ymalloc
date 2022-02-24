#include "heap.h"

static void* heapBase = NULL;
static size_t heapSize = 0;

void* HeapBegin(void) { return heapBase; }
void* HeapEnd(void) { return ((uint8_t*) heapBase) + heapSize; }

BlockNode* InitBlock(void* ptr, size_t size, BlockUsage usage) {
    BlockSize* header = (BlockSize*) (((uint8_t*) ptr));
    BlockSize* footer = (BlockSize*) (((uint8_t*) ptr) + BLOCK_HEADER_SIZE + size);
    BlockSize blockSize = usage == BLOCK_USED ? BLOCKSIZE_ALLOC(size) : BLOCKSIZE_FREE(size);
    *header = blockSize;
    *footer = blockSize;
    BlockNode* payload = (BlockNode*) (((uint8_t*) ptr) + BLOCK_HEADER_SIZE);
    return payload;
}

void* HeapInit(void) {
    // don't init again
    if (heapBase)
        return heapBase;

    // get the current break
    void* base = sbrk(0);
    if (!SBRK_OK(base))
        return NULL;

    if (!HeapGrow(HEAP_INIT_SIZE))
        return NULL;

    return heapBase = base;
}

// grows the heap and creates a free block
BlockSize* HeapGrow(size_t size) {
    // try to grow the heap
    size_t payloadSize = PAYLOAD_ALIGN(size);
    size_t blockSize = payloadSize + BLOCK_AUXILIARY_SIZE;
    void* blockPtr = sbrk(blockSize);
    if (!SBRK_OK(blockPtr))
        return NULL;
    
    // heap grow success
    heapSize += blockSize;

    // create a free block in the new space
    BlockNode* node = InitBlock(blockPtr, payloadSize, BLOCK_FREE);
    node->next = NULL;
    node->prev = NULL;
    return blockPtr;
}
