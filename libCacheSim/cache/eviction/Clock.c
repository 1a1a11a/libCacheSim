

#include "../../include/libCacheSim/evictionAlgo/Clock.h"

#include "../../dataStructure/hashtable/hashtable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *pointer;
} Clock_params_t;

void Clock_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove);

cache_t *Clock_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("Clock", ccache_params);
  cache->cache_init = Clock_init;
  cache->cache_free = Clock_free;
  cache->get = Clock_get;
  cache->check = Clock_check;
  cache->insert = Clock_insert;
  cache->evict = Clock_evict;
  cache->remove = Clock_remove;
  cache->to_evict = Clock_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->per_obj_metadata_size = 1;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  if (cache_specific_params != NULL) {
    ERROR("Clock does not support any parameters, but got %s\n",
          cache_specific_params);
    abort();
  }

  cache->eviction_params = my_malloc(Clock_params_t);
  ((Clock_params_t *)(cache->eviction_params))->pointer = NULL;

  return cache;
}

void Clock_free(cache_t *cache) { cache_struct_free(cache); }

cache_ck_res_e Clock_check(cache_t *cache, const request_t *req,
                           const bool update_cache) {
  cache_obj_t *cache_obj;
  cache_ck_res_e res = cache_check_base(cache, req, update_cache, &cache_obj);
  if (cache_obj != NULL) cache_obj->clock.visited = true;

  return res;
}

cache_ck_res_e Clock_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_obj_t *Clock_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *cached_obj = cache_insert_LRU(cache, req);
  cached_obj->clock.visited = false;

  return cached_obj;
}

cache_obj_t *Clock_to_evict(cache_t *cache) {
  Clock_params_t *params = cache->eviction_params;
  cache_obj_t *moving_pointer = params->pointer;

  /* if we have run one full around or first eviction */
  if (moving_pointer == NULL) moving_pointer = cache->q_tail;

  /* find the first untouched */
  while (moving_pointer != NULL && moving_pointer->clock.visited) {
    moving_pointer->clock.visited = false;
    moving_pointer = moving_pointer->queue.prev;
  }

  /* if we have finished one around, start from the tail */
  if (moving_pointer == NULL) {
    moving_pointer = cache->q_tail;
    while (moving_pointer != NULL && moving_pointer->clock.visited) {
      moving_pointer->clock.visited = false;
      moving_pointer = moving_pointer->queue.prev;
    }
  }
  return moving_pointer;
}

void Clock_evict(cache_t *cache, const request_t *req,
                 cache_obj_t *evicted_obj) {
  Clock_params_t *params = cache->eviction_params;
  cache_obj_t *moving_pointer = Clock_to_evict(cache);

  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, moving_pointer, sizeof(cache_obj_t));
  }

  params->pointer = moving_pointer->queue.prev;
  Clock_remove_obj(cache, moving_pointer);
}

void Clock_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  if (obj_to_remove == NULL) {
    ERROR("remove NULL from cache\n");
    abort();
  }
  remove_obj_from_list(&cache->q_head, &cache->q_tail, obj_to_remove);
  cache_remove_obj_base(cache, obj_to_remove);
}

void Clock_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    PRINT_ONCE("remove object %" PRIu64 "that is not cached\n", obj_id);
    return;
  }
  Clock_remove_obj(cache, obj);
}

#ifdef __cplusplus
}
#endif
