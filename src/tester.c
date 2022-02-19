#include "ymalloc.h"

#include <stdio.h>

int main() {
    printf("%lu\n", sizeof(void*));

    void* x = ymalloc(100);
    // *(size_t*) (x) = 2; // ...
    yfree(x);
}
