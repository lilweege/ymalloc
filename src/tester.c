#include "ymalloc.h"

#include <stdio.h>
#include <string.h>

int main() {

    void* x = ymalloc(30);
    void* y = ymalloc(1);
    void* z = ymalloc(100);
    memset(x, 'A', 30);
    memset(y, 'B', 1);
    memset(z, 'C', 100);

    yrealloc(NULL, 0);
    printf("free y\n");
    yfree(y);
    yrealloc(NULL, 0);
    printf("free x\n");
    yfree(x);
    yrealloc(NULL, 0);

    printf("alloc w\n");
    void* w = ymalloc(20);
    memset(w, 'D', 20);
    yrealloc(NULL, 0);

    printf("free z\n");
    yfree(z);
    yrealloc(NULL, 0);
    printf("free w\n");
    yfree(w);
    yrealloc(NULL, 0);

    // size_t* arr = ((size_t*) x) - 1;
    // for (size_t i = 0; i <= (HEAP_INIT_SIZE)/sizeof(size_t); ++i)
    //     printf("%zu: %zu (0x%zx)\n", i, arr[i], arr[i]);

    // size_t i = 1 + (HEAP_INIT_SIZE)/sizeof(size_t);
    // printf("%zu (end): %zu\n", i, arr[i]);

}
