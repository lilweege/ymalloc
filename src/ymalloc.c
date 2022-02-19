#include "ymalloc.h"

void* ymalloc(size_t size) {
    
    // nothing to allocate
    if (size == 0)
        return NULL;
    size = ALIGN_UP(size, HEAP_ALIGN);

    // TODO ...
    return NULL;
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
