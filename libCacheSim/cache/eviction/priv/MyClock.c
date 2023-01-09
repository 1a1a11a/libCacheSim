

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/priv/MyClock.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  cache_obj_t *pointer;
  int64_t n_scanned_obj;
  int64_t n_written_obj;
  double wrap_around_ratio;
} MyClock_params_t;


cache_t *MyClock_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("MyClock", ccache_params);
  cache->cache_init = MyClock_init;
  cache->cache_free = MyClock_free;
  cache->get = MyClock_get;
  cache->check = MyClock_check;
  cache->insert = MyClock_insert;
  cache->evict = MyClock_evict;
  cache->remove = MyClock_remove;
  cache->to_evict = MyClock_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 1;
  } else {
    cache->obj_md_size = 0;
  }

  if (cache_specific_params != NULL) {
    ERROR("Clock does not support any parameters, but got %s\n",
          cache_specific_params);
    abort();
  }

  cache->eviction_params = my_malloc(MyClock_params_t);
  MyClock_params_t *params = (MyClock_params_t *) cache->eviction_params;
  params->pointer = NULL;
  params->q_head = NULL;
  params->q_tail = NULL;

  // params->n_scanned_obj = 0;
  // params->n_written_obj = 0;
  // params->wrap_around_ratio = 0.80;
  // snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "Clock-%.2lf",
  //          params->wrap_around_ratio);

  return cache;
}

void MyClock_free(cache_t *cache) { cache_struct_free(cache); }

bool MyClock_check(cache_t *cache, const request_t *req,
                 const bool update_cache) {
  cache_obj_t *cache_obj;
  bool cache_hit = cache_check_base(cache, req, update_cache, &cache_obj);
  if (cache_obj != NULL && update_cache) cache_obj->clock.visited = true;

  return cache_hit;
}

bool MyClock_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_obj_t *MyClock_insert(cache_t *cache, const request_t *req) {
  MyClock_params_t *params = cache->eviction_params;
  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);
  obj->clock.visited = false;
  params->n_written_obj++;

  return obj;
}

cache_obj_t *MyClock_to_evict(cache_t *cache) {
  MyClock_params_t *params = cache->eviction_params;
  cache_obj_t *pointer = params->pointer;

  /* if we have run one full around or first eviction */
  if (pointer == NULL) pointer = params->q_tail;

  // if (params->n_scanned_obj >= params->n_written_obj * params->wrap_around_ratio) {
  //   pointer = params->q_tail;
  //   params->n_scanned_obj = 0;
  //   params->n_written_obj -= params->n_scanned_obj;
  // }

  /* find the first untouched */
  while (pointer != NULL && pointer->clock.visited) {
    params->n_scanned_obj++;
    pointer->clock.visited = false;
    pointer = pointer->queue.prev;
  }

  /* if we have finished one around, start from the tail */
  if (pointer == NULL) {
    // printf("%ld %ld\n", params->n_scanned_obj, params->n_written_obj);
    pointer = params->q_tail;
    while (pointer != NULL && pointer->clock.visited) {
      pointer->clock.visited = false;
      pointer = pointer->queue.prev;
    }
  }

  params->n_scanned_obj++;
  return pointer;
}

void MyClock_evict(cache_t *cache, const request_t *req,
                 cache_obj_t *evicted_obj) {
  MyClock_params_t *params = cache->eviction_params;
  cache_obj_t *moving_pointer = MyClock_to_evict(cache);

  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, moving_pointer, sizeof(cache_obj_t));
  }

  params->pointer = moving_pointer->queue.prev;
  remove_obj_from_list(&params->q_head, &params->q_tail, moving_pointer);
  cache_evict_base(cache, moving_pointer, true);
}

void MyClock_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  DEBUG_ASSERT(obj_to_remove != NULL);
  MyClock_params_t *params = cache->eviction_params;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_remove);
  cache_remove_obj_base(cache, obj_to_remove, true);
}

bool MyClock_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }
  MyClock_remove_obj(cache, obj);

  return true;
}

#ifdef __cplusplus
}
#endif
