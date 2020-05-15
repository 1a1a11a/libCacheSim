//
// Created by Juncheng Yang on 5/13/20.
//

#ifndef LIBMIMIRCACHE_MEM_H
#define LIBMIMIRCACHE_MEM_H

/** one problem with this memory allocator is once memory is allocated, it won't be able to take back
 * this probably won't be a problem in our use case
 *
 **/


#ifdef __cplusplus
extern "C"
{
#endif

#include <glib.h>
#include "../../include/mimircache/cacheObj.h"


#define N_OBJ_MEM_ARENA (1000*1000L)
#define MEM_ARENA_SIZE (sizeof(cache_obj_t) * N_OBJ_MEM_ARENA)
#define N_MAX_ARENA (65535)
// max mem allocable is MEM_ARENA_SIZE * N_MAX_ARENA ~ 1000 GB - this should be more than enough for now




typedef struct {
  cache_obj_t *cache_obj_mem_arena[N_MAX_ARENA];
  GQueue *free_list;
  gint32 n_arena;
  gint32 idx_in_cur_arena;     // points to the pos of next allocation

} mem_allocator_t;


static inline void init_mem_alloc(mem_allocator_t *mem_alloc){
  mem_alloc->cache_obj_mem_arena[0] = g_new0(cache_obj_t, N_OBJ_MEM_ARENA);
  mem_alloc->n_arena = 1;
  mem_alloc->idx_in_cur_arena = 0;
  mem_alloc->free_list = g_queue_new();
}

static inline void free_mem_alloc(mem_allocator_t *mem_alloc) {
  for (gint32 i=0; i<mem_alloc->n_arena; i++){
    g_free(mem_alloc->cache_obj_mem_arena[i]);
  }
  g_queue_free(mem_alloc->free_list);
}

static inline cache_obj_t* new_cache_obj(mem_allocator_t *mem_alloc) {

  cache_obj_t *cache_obj;
  if (mem_alloc->free_list->length > 0){
    cache_obj = (cache_obj_t *) g_queue_pop_head(mem_alloc->free_list);
  }
  else{
    cache_obj = &mem_alloc->cache_obj_mem_arena[mem_alloc->n_arena][mem_alloc->idx_in_cur_arena++];
    if (mem_alloc->idx_in_cur_arena == N_OBJ_MEM_ARENA){
      // need one more arena
      mem_alloc->cache_obj_mem_arena[++mem_alloc->n_arena] = g_new0(cache_obj_t, N_OBJ_MEM_ARENA);
      mem_alloc->idx_in_cur_arena = 0;
    }
  }

  return cache_obj;
}

static inline void free_cache_obj(mem_allocator_t* mem_alloc, cache_obj_t* cache_obj) {
  g_queue_push_tail(mem_alloc->free_list, cache_obj);
}


#ifdef __cplusplus
}
#endif

#endif //LIBMIMIRCACHE_MEM_H
