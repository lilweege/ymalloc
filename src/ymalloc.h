// external header api

#ifndef YMALLOC_H
#define YMALLOC_H

#include "heap.h"

void* ymalloc(size_t size);
void* yrealloc(void* prt, size_t size);
void yfree(void* ptr);

#endif // YMALLOC_H
