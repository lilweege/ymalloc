#include "heap.h"

Block* heapStart = NULL;

// initialize heapStart
bool HeapInit() {

    // don't init again
    if (heapStart)
        return false;

    // get the current break
    void* base = sbrk(0);
    if (!SBRK_OK(base))
        return false;

    // try to get a block
    if (!SBRK_OK(sbrk(sizeof(Block) + HEAP_INIT_SIZE)))
        return false;

    // set the heap base ptr
    heapStart = (Block*) base;
    heapStart->prev = NULL;
    heapStart->next = NULL;
    heapStart->size = HEAP_INIT_SIZE;

    return true;
}

bool HeapGrow(size_t size) {
    size = ALIGN_UP(size, 32);
    return false;
}
