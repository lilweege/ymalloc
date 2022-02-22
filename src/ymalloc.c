#include "ymalloc.h"

static BlockSize* freeListHead = NULL;

static BlockSize* FindBlock(size_t size) {
    if (!freeListHead)
        freeListHead = HeapInit();
    if (!freeListHead)
        assert(0 && "ERROR: Failed to initialize heap!");
    
    // TODO ...
    (void) size;

    return freeListHead;
}

void* ymalloc(size_t size) {
    // nothing to allocate
    if (size == 0)
        return NULL;

    size = PAYLOAD_ALIGN(size);
    
    // TODO ...
    return FindBlock(size);
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
