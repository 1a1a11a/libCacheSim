//
// Created by Juncheng Yang on 5/13/20.
//

#ifndef libCacheSim_MEM_H_DEP
#define libCacheSim_MEM_H_DEP

/** one problem with this memory allocator is once memory is allocated, it won't be able to take back
 * this probably won't be a problem in our use case
 * NOTE this is not MT-safe, use it as thread-local allocator
 **/


#ifdef __cplusplus
extern "C"
{
#endif

#include <glib.h>

#include <pthread.h>
#include <sys/mman.h>
#include "libCacheSim/logging.h"


#define N_OBJ_MEM_ARENA (1024*1024L)
#define MEM_ARENA_SIZE (sizeof(cache_obj_t) * N_OBJ_MEM_ARENA)
#define MEM_ARENA_SIZE_SLAB_OBJ (sizeof(slab_cache_obj_t) * N_OBJ_MEM_ARENA)
#define N_MAX_ARENA (65535)
// max mem allocable is MEM_ARENA_SIZE * N_MAX_ARENA ~ 1000 GB - this should be more than enough for now
#define N_MEM_ALLOCATOR 4 // to avoid lock contention


typedef struct {
  cache_obj_t *cache_obj_mem_arena[N_MAX_ARENA];
  GQueue *free_list;
  gint32 cur_arena_idx;
  gint32 idx_in_cur_arena;     // points to the pos of next allocation
  pthread_mutex_t mtx;

} mem_allocator_t;

typedef struct {
  slab_cache_obj_t *cache_obj_mem_arena[N_MAX_ARENA];
  GQueue *free_list;
  gint32 cur_arena_idx;
  gint32 idx_in_cur_arena;     // points to the pos of next allocation
  pthread_mutex_t mtx;
} mem_allocator_slab_t;




void init_mem_alloc(mem_allocator_t *mem_alloc);

void free_mem_alloc(mem_allocator_t *mem_alloc);

cache_obj_t *new_cache_obj();

void free_cache_obj(cache_obj_t *cache_obj);


void init_all_global_mem_alloc();

void free_all_global_mem_alloc();


#ifdef __cplusplus
}
#endif

#endif //libCacheSim_MEM_H_DEP
