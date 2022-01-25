//
// Created by Juncheng Yang on 5/15/20.
//


#include "dep/mem.h.dep"
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif


static mem_allocator_t global_mem_alloc[N_MEM_ALLOCATOR];
static gboolean global_mem_alloc_is_initialized = FALSE;
static unsigned long long n_arena_alloc = 0;
static unsigned long long n_free_list_alloc = 0;
//static guint64 n_arena_alloc = 0;
//static guint64 n_arena_alloc = 0;
//static guint64 n_arena_alloc = 0;

static inline void* _allocate_memory(guint64 sz){
  void *mem_region;
  static gboolean error_reported = FALSE;
#ifdef __linux__
  mem_region = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (mem_region == MAP_FAILED){
    if (! error_reported){
      error_reported = TRUE;
      INFO("allocating %lu byte memory\n", MEM_ARENA_SIZE);
      WARN("unable to use huge page");
      perror(": ");
    }
    mem_region = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  }
  madvise(mem_region, MEM_ARENA_SIZE, MADV_HUGEPAGE);
#else
  if (! error_reported){
    error_reported = TRUE;
    WARN("system does not support huge page\n");
  }
  mem_region = mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
  return mem_region;
}


void init_all_global_mem_alloc() {
  if (global_mem_alloc_is_initialized)
    return;
  global_mem_alloc_is_initialized = TRUE;
  for (int i = 0; i < N_MEM_ALLOCATOR; i++) {
    init_mem_alloc(&global_mem_alloc[i]);
  }
//  printf("initialized %d %p\n", global_mem_alloc_is_initialized, &global_mem_alloc_is_initialized);
  INFO("myalloc - arena size %.2lf MB, %d allocator\n", MEM_ARENA_SIZE/1024.0/1024, N_MEM_ALLOCATOR);
}

void free_all_global_mem_alloc() {
  global_mem_alloc_is_initialized = FALSE;
  unsigned long n_free_list_entry = 0;
  for (int i = 0; i < N_MEM_ALLOCATOR; i++) {
    n_free_list_entry += global_mem_alloc[i].free_list->length;
    free_mem_alloc(&global_mem_alloc[i]);
  }

  INFO("myalloc - %llu arena alloc %llu free_list alloc, free_list size %lu\n", n_arena_alloc, n_free_list_alloc, n_free_list_entry);
}

void init_mem_alloc(mem_allocator_t *mem_alloc) {
  pthread_mutex_init(&mem_alloc->mtx, NULL);
  pthread_mutex_lock(&mem_alloc->mtx);
//  mem_alloc->cache_obj_mem_arena[0] = g_new0(cache_obj_t, N_OBJ_MEM_ARENA);
//  mem_alloc->cache_obj_mem_arena[0] = (cache_obj_t*) malloc(MEM_ARENA_SIZE);

  mem_alloc->cache_obj_mem_arena[0] = (cache_obj_t*) _allocate_memory(MEM_ARENA_SIZE);
  mem_alloc->cur_arena_idx = 0;
  mem_alloc->idx_in_cur_arena = 0;
  mem_alloc->free_list = g_queue_new();
  VERBOSE("init mem_alloc %p - free_list %p\n", mem_alloc, mem_alloc->free_list);
  pthread_mutex_unlock(&mem_alloc->mtx);
}

void free_mem_alloc(mem_allocator_t *mem_alloc) {
  pthread_mutex_lock(&mem_alloc->mtx);
  for (gint32 i = 0; i < mem_alloc->cur_arena_idx+1; i++) {
//    g_free(mem_alloc->cache_obj_mem_arena[i]);
//    free(mem_alloc->cache_obj_mem_arena[i]);
    munmap(mem_alloc->cache_obj_mem_arena[i], MEM_ARENA_SIZE);
  }
  g_queue_free(mem_alloc->free_list);
  pthread_mutex_unlock(&mem_alloc->mtx);
  pthread_mutex_destroy(&mem_alloc->mtx);
}



cache_obj_t *new_cache_obj() {
//  printf("initialized? %d %p\n", global_mem_alloc_is_initialized, &global_mem_alloc_is_initialized);
  if (!global_mem_alloc_is_initialized) {
    ERROR("custom memory allocator enabled, but haven't called init_all_global_mem_alloc() to initialize allocator\n");
    abort();
  }
  cache_obj_t *cache_obj;
  mem_allocator_t* mem_alloc = &global_mem_alloc[ (guint64) (pthread_self()) % N_MEM_ALLOCATOR];
  pthread_mutex_lock(&mem_alloc->mtx);

  if (mem_alloc->free_list->length > 0) {
    cache_obj = (cache_obj_t *) g_queue_pop_head(mem_alloc->free_list);
    n_free_list_alloc += 1;
  } else {
    cache_obj = &mem_alloc->cache_obj_mem_arena[mem_alloc->cur_arena_idx][mem_alloc->idx_in_cur_arena++];
//    printf("here cur_arena_idx %d %p\n", mem_alloc->cur_arena_idx, mem_alloc->cache_obj_mem_arena[mem_alloc->cur_arena_idx]);
    if (mem_alloc->idx_in_cur_arena == N_OBJ_MEM_ARENA) {
      n_arena_alloc += 1;
      // need one more arena
//      mem_alloc->cache_obj_mem_arena[++mem_alloc->cur_arena_idx] = g_new0(cache_obj_t, N_OBJ_MEM_ARENA);
      mem_alloc->cache_obj_mem_arena[++mem_alloc->cur_arena_idx] = (cache_obj_t*) _allocate_memory(MEM_ARENA_SIZE);
      mem_alloc->idx_in_cur_arena = 0;
    }
  }

  pthread_mutex_unlock(&mem_alloc->mtx);
  return cache_obj;
}

void free_cache_obj(cache_obj_t *cache_obj) {
  mem_allocator_t* mem_alloc = &global_mem_alloc[ (guint64) (pthread_self()) % N_MEM_ALLOCATOR];
  pthread_mutex_lock(&mem_alloc->mtx);
  g_queue_push_tail(mem_alloc->free_list, cache_obj);
  pthread_mutex_unlock(&mem_alloc->mtx);
}


slab_cache_obj_t *new_slab_cache_obj() {
//  printf("initialized? %d %p\n", global_mem_alloc_is_initialized, &global_mem_alloc_is_initialized);
  if (!global_mem_alloc_is_initialized) {
    ERROR("custom memory allocator enabled, but haven't called init_all_global_mem_alloc() to initialize allocator\n");
    abort();
  }
  slab_cache_obj_t *cache_obj;
  mem_allocator_t* mem_alloc = &global_mem_alloc[ (guint64) (pthread_self()) % N_MEM_ALLOCATOR];
  pthread_mutex_lock(&mem_alloc->mtx);

  if (mem_alloc->free_list->length > 0) {
    cache_obj = (slab_cache_obj_t *) g_queue_pop_head(mem_alloc->free_list);
    n_free_list_alloc += 1;
  } else {
    cache_obj = (slab_cache_obj_t*) &mem_alloc->cache_obj_mem_arena[mem_alloc->cur_arena_idx][sizeof(slab_cache_obj_t) * (mem_alloc->idx_in_cur_arena++)];
//    printf("here cur_arena_idx %d %p\n", mem_alloc->cur_arena_idx, mem_alloc->cache_obj_mem_arena[mem_alloc->cur_arena_idx]);
    if (mem_alloc->idx_in_cur_arena == N_OBJ_MEM_ARENA) {
      n_arena_alloc += 1;
      // need one more arena
      mem_alloc->cache_obj_mem_arena[++mem_alloc->cur_arena_idx] = (void*) _allocate_memory(MEM_ARENA_SIZE_SLAB_OBJ);
      mem_alloc->idx_in_cur_arena = 0;
    }
  }

  pthread_mutex_unlock(&mem_alloc->mtx);
  return cache_obj;
}

void free_slab_cache_obj(void *cache_obj) {
  mem_allocator_t* mem_alloc = &global_mem_alloc[ (guint64) (pthread_self()) % N_MEM_ALLOCATOR];
  pthread_mutex_lock(&mem_alloc->mtx);
  g_queue_push_tail(mem_alloc->free_list, cache_obj);
  pthread_mutex_unlock(&mem_alloc->mtx);
}





#ifdef __cplusplus
}
#endif


