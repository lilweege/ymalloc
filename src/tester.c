#include "ymalloc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*
void MallocFreeTest1() {
    void* x = ymalloc(20);
    void* y = ymalloc(20);
    void* z = ymalloc(20);
    DumpFreeList();
    // xx yy zz FFFFFF

    yfree(x);
    DumpFreeList();
    // FF yy zz FFFFFF
    
    yfree(y);
    DumpFreeList();
    // FFFF zz FFFFFF

    yfree(z);
    DumpFreeList();
    // FFFFFFFFFFFF
}

void MallocFreeTest2() {
    void* x = ymalloc(4096-16);
    DumpFreeList();
    // xxxxxxxxx

    void* y = ymalloc(69);
    DumpFreeList();
    // xxxxxxxxx yy

    yfree(x);
    DumpFreeList();
    // FFFFFFFFF yy

    yfree(y);
    DumpFreeList();
    // FFFFFFFFFFF
}

void MallocFreeTest3() {
    void* x = ymalloc(20);
    DumpFreeList();
    // xx FFFFFFFFF

    void* y = ymalloc(5000);
    DumpFreeList();
    // xx yyyyyyyy FFFFFFFFF
    
    void* z = ymalloc(20);
    DumpFreeList();
    // xx yyyyyyyy zz FFFFFFF

    void* w = ymalloc(20);
    DumpFreeList();
    // xx yyyyyyyy zz ww FFFFF

    yfree(z);
    DumpFreeList();
    // xx yyyyyyyy FF ww FFFFF

    yfree(w);
    DumpFreeList();
    // xx yyyyyyyy FFFFFFFFF

    yfree(x);
    DumpFreeList();
    // FF yyyyyyyy FFFFFFFFF

    yfree(y);
    DumpFreeList();
    // FFFFFFFFFFFFFFFFFFF
}

void MallocFreeTest4() {
    void* a = ymalloc(20);
    void* b = ymalloc(30);
    void* c = ymalloc(40);
    void* d = ymalloc(50);
    void* e = ymalloc(60);
    void* f = ymalloc(70);
    DumpFreeList();
    // aa bb cc dd ee ff FFFFFFF

    yfree(a);
    yfree(c);
    
    DumpFreeList();
    // FF bb FF dd ee ff FFFFFFF

    yfree(b);
    DumpFreeList();
    // FFFFFF dd ee ff FFFFFFF

    yfree(d);
    DumpFreeList();
    // FFFFFFFF ee ff FFFFFFF

    yfree(f);
    DumpFreeList();
    // FFFFFFFF ee FFFFFFFFF

    yfree(e);
    DumpFreeList();
    // FFFFFFFF ee FFFFFFFFF
}

void MallocFreeTest5() {
    void* x = ymalloc(30);
    void* y = ymalloc(70);
    void* z = ymalloc(150);
    DumpFreeList();
    // xx yyy zzzz FFFFFF
    
    yfree(y);
    DumpFreeList();
    // xx FFF zzzz FFFFFF

    void* big = ymalloc(5000);
    DumpFreeList();
    // xx FFF zzzz bbbbbbb FFFFFF

    void* a = ymalloc(40);
    DumpFreeList();
    // xx aa F zzzz bbbbbbb FFFFFF
    
    yfree(x);
    DumpFreeList();
    // FF aa F zzzz bbbbbbb FFFFFF
    
    yfree(a);
    DumpFreeList();
    // FFFFF zzzz bbbbbbb FFFFFF

    yfree(big);
    DumpFreeList();
    // FFFFF zzzz FFFFFFFFFFFFF

    yfree(z);
    DumpFreeList();
    // FFFFFFFFFFFFFFFFFFFFFF
}

void ReallocTest1() {
    void* go = ymalloc(50);
    void* stop = ymalloc(1);
    yfree(go);
    DumpFreeList();
    // FF s FFFFFFFFF

    void* x = ymalloc(100);
    DumpFreeList();
    // FF s xxx FFFFFF

    assert(x == yrealloc(x, 90)); // no shrink
    assert(x == yrealloc(x, 80)); // no shrink
    assert(x == yrealloc(x, 70)); // shrink
    DumpFreeList();
    // FF s xx FFFFFFF

    yfree(stop);
    DumpFreeList();
    // FFF xx FFFFFFF

    // printf("realloc 200\n");
    // x = yrealloc(x, 200);
}

void ReallocTest2() {
    void* x = ymalloc(16);
    void* a = ymalloc(32); memset(a, 'A', 32);
    void* b = ymalloc(16);
    void* c = ymalloc(16);
    // switch order of these frees to test splice
    yfree(b);
    yfree(x);
    DumpFreeList();
    // F aa F c FFFFFF

    assert(a == yrealloc(a, 64));
    DumpFreeList();
    // F aaaa c FFFFFF

    for (int i = 0; i < 32; ++i)
        assert(((char*)a)[i] == 'A');
    assert(*(size_t*)(((char*)a) - 8) == 64);
    assert(*(size_t*)(((char*)a) + 64) == 64);

    yfree(a);
    yfree(c);
}

void ReallocTest3() {
    void* x = ymalloc(16);
    void* a = ymalloc(32); memset(a, 'A', 32);
    void* b = ymalloc(16);
    yfree(b);
    yfree(x);
    DumpFreeList();
    // F aa FFFFFFFF

    // size_t newSize = 4016;
    size_t newSize = 40;
    assert(a == yrealloc(a, newSize));
    DumpFreeList();
    // F aaaa FFFFFF

    for (int i = 0; i < 32; ++i)
        assert(((char*)a)[i] == 'A');
    assert(*(size_t*)(((char*)a) - 8) == newSize);
    assert(*(size_t*)(((char*)a) + newSize) == newSize);

    yfree(a);
}

int main() {
    // MallocFreeTest1();
    // MallocFreeTest2();
    // MallocFreeTest3();
    // MallocFreeTest4();
    // MallocFreeTest5();

    // ReallocTest1();
    // ReallocTest2();
    ReallocTest3();
}
*/

#define NUM_ITERATIONS   50000
#define MAX_ALLOCATIONS  500
#define MIN_ALLOC_SIZE   32
#define MAX_ALLOC_SIZE   1024

typedef void* (*malloc_like_func)(size_t);
typedef void (*free_like_func)(void*);

malloc_like_func malloc_fp = NULL;
free_like_func free_fp = NULL;


void* log_alloc(size_t size,  FILE* fp) {
    void* p = malloc_fp(size);
    fprintf(fp, "M, %lu, %lu\n", (uintptr_t)p, size);
    return p;
}

void log_free(void* p, FILE* fp) {
    fprintf(fp, "F, %lu\n", (uintptr_t)p);
    free_fp(p);
}

typedef struct {
    void* ptr;
    char isAllocated;
} Allocation;

void doAllocations(const char* filename, int indices[NUM_ITERATIONS]) {
    FILE* fp = fopen(filename, "w");
    if (fp == NULL) return;
    Allocation allocations[MAX_ALLOCATIONS];
    memset(allocations, 0, sizeof(allocations));
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        int idx = indices[i];
        if (allocations[idx].isAllocated) {
            allocations[idx].isAllocated = 0;
            log_free(allocations[idx].ptr, fp);
        }
        else {
            allocations[idx].isAllocated = 1;
            size_t allocSize = rand() % (MAX_ALLOC_SIZE - MIN_ALLOC_SIZE) + MIN_ALLOC_SIZE;
            allocations[idx].ptr = log_alloc(allocSize, fp);
        }
    }

    for (int i = 0; i < MAX_ALLOCATIONS; ++i) {
        if (allocations[i].isAllocated) {
            log_free(allocations[i].ptr, fp);
        }
    }
    fclose(fp);
}

int main() {
    srand(time(NULL));
    // srand(0);

    int indices[NUM_ITERATIONS];
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        indices[i] = rand() % MAX_ALLOCATIONS;
    }

    malloc_fp = ymalloc; free_fp = yfree;
    doAllocations("ymalloc.log", indices);
    malloc_fp =  malloc; free_fp =  free;
    doAllocations("malloc.log", indices);
}
