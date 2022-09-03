#include "ymalloc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>



#define SAVE_LOG         0
#define NUM_ITERATIONS   500000
#define MAX_ALLOCATIONS  500
#define MIN_ALLOC_SIZE   32
#define MAX_ALLOC_SIZE   1024
// #define MAX_ALLOC_SIZE   (1024*100)

typedef void* (*malloc_like_func)(size_t);
typedef void (*free_like_func)(void*);

malloc_like_func malloc_fp = NULL;
free_like_func free_fp = NULL;


void* log_alloc(size_t size,  FILE* f) {
    (void) f;
    void* p = malloc_fp(size);
#if SAVE_LOG
    fprintf(f, "M, %lu, %lu\n", (uintptr_t)p, size);
#endif
    return p;
}

void log_free(void* p, FILE* f) {
    (void) f;
#if SAVE_LOG
    fprintf(f, "F, %lu\n", (uintptr_t)p);
#endif
    free_fp(p);
}

typedef struct {
    void* ptr;
    size_t size;
    uint8_t pattern;
    bool isAllocated;
} Allocation;

int indices[NUM_ITERATIONS];

clock_t doRandomAllocations(const char* logfile) {
#if SAVE_LOG
    FILE* fp = fopen(logfile, "w");
    if (fp == NULL) return 0;
#else
    (void) logfile;
    FILE* fp = NULL;
#endif
    Allocation allocations[MAX_ALLOCATIONS];
    memset(allocations, 0, sizeof(allocations));

    uint8_t currentPattern = 0;
    clock_t t0 = clock();
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        int idx = indices[i];
        if (allocations[idx].isAllocated) {
#ifdef DEBUG
            for (size_t i = 0; i < allocations[idx].size; ++i) {
                uint8_t* addr = &(((uint8_t*)(allocations[idx].ptr))[i]);
                if (*addr != allocations[idx].pattern) {
                    fprintf(stderr, "Error: Corrupted memory at %p, expected %#04X, got %#04X\n",
                        addr, *addr, allocations[idx].pattern);
                    exit(1);
                }
            }
#endif
            allocations[idx].isAllocated = false;
            log_free(allocations[idx].ptr, fp);
        }
        else {
            allocations[idx].pattern = currentPattern++;
            allocations[idx].isAllocated = true;
            size_t allocSize = rand() % (MAX_ALLOC_SIZE - MIN_ALLOC_SIZE) + MIN_ALLOC_SIZE;
            allocations[idx].size = allocSize;
            allocations[idx].ptr = log_alloc(allocSize, fp);
#ifdef DEBUG
            memset(allocations[idx].ptr, allocations[idx].pattern, allocSize);
#endif
        }
    }

    for (int i = 0; i < MAX_ALLOCATIONS; ++i) {
        if (allocations[i].isAllocated) {
            log_free(allocations[i].ptr, fp);
        }
    }
    clock_t t1 = clock();

#if SAVE_LOG
    fclose(fp);
#endif
    return t1-t0;
}

int main() {
    // srand(time(NULL));
    srand(0);
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        indices[i] = rand() % MAX_ALLOCATIONS;
    }

    malloc_fp = ymalloc; free_fp = yfree;
    clock_t t1 = doRandomAllocations("ymalloc.log");
    malloc_fp =  malloc; free_fp =  free;
    clock_t t2 = doRandomAllocations("malloc.log");

    printf(
        "ymalloc: %20ldus\n"
        "malloc:  %20ldus\n", t1, t2);
    return 0;
}
