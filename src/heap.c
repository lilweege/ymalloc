#include "heap.h"

static void* heapBase = NULL;

void InitBlock(void* ptr, size_t size) {
    BlockSize* header = (BlockSize*) (((uint8_t*) ptr));
    BlockSize* footer = (BlockSize*) (((uint8_t*) ptr) + BLOCK_HEADER_SIZE + size);
    BlockSize blockSize = BLOCKSIZE_FREE(size);
    *header = blockSize;
    *footer = blockSize;
    BlockNode* payload = (BlockNode*) (((uint8_t*) ptr) + BLOCK_HEADER_SIZE);
    payload->next = NULL;
    payload->prev = NULL;
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

size_t HeapGrow(size_t size) {
    size_t payloadSize = PAYLOAD_ALIGN(size);
    size_t blockSize = payloadSize + BLOCK_AUXILIARY_SIZE;
    void* blockPtr = sbrk(blockSize);
    if (!SBRK_OK(blockPtr))
        return 0;
    InitBlock(blockPtr, payloadSize);
    return blockSize;
}
