//
// Created by Juncheng Yang on 6/9/20.
//

#ifndef libCacheSim_MEM_H
#define libCacheSim_MEM_H

#include "../config.h"

#if HEAP_ALLOCTOR == HEAP_ALLOCATOR_G_NEW
#include "glib.h"
#define my_malloc(type) g_new(type, 1)
#define my_malloc_n(type, n) g_new(type, n)
#define my_free(size, addr) g_free(addr)

#elif HEAP_ALLOCATOR == HEAP_ALLOCATOR_G_SLICE_NEW
#include "gmodule.h"
#define my_malloc(type) g_slice_new(type)
#define my_malloc_n(type, n) (type *)g_slice_alloc(sizeof(type) * n)
#define my_free(size, addr) g_slice_free1(size, addr)

#elif HEAP_ALLOCATOR == HEAP_ALLOCATOR_MALLOC
#include <stdlib.h>
#define my_malloc(type) (type *)malloc(sizeof(type))
#define my_malloc_n(type, n) (type *)calloc(sizeof(type), n)
#define my_free(size, addr) free(addr)

#elif HEAP_ALLOCATOR == HEAP_ALLOCATOR_ALIGNED_MALLOC
#include <stdlib.h>
#define my_malloc(type) (type *)aligned_alloc(MEM_ALIGN_SIZE, sizeof(type));
#define my_malloc_n(type, n) \
  (type *)aligned_alloc(MEM_ALIGN_SIZE, sizeof(type) * n)
#define my_free(size, addr) free(addr)
#endif

#endif  // libCacheSim_MEM_H
