// external header api

#ifndef YMALLOC_H
#define YMALLOC_H

#include "heap.h"

void* ymalloc(size_t size);
void yfree(void* ptr);
void* ycalloc(size_t nmemb, size_t size);
void* yrealloc(void* ptr, size_t size);

#endif // YMALLOC_H
