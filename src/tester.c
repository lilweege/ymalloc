#include "ymalloc.h"

#include <stdio.h>

int main() {

    void* x = ymalloc(100);
    HeapGrow(1);
    HeapGrow(18);
    size_t* arr = ((size_t*) x) + 1;
    for (size_t i = 0; i < 10 + (HEAP_INIT_SIZE)/sizeof(size_t); ++i)
        printf("%zu: %zu\n", i, arr[i]);

    // size_t i = (HEAP_INIT_SIZE)/sizeof(size_t);
    // printf("end %zu\n", arr[i]);


    // yfree(x);
}
