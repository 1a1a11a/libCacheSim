/* Hyperbolic caching */

#include "../include/libCacheSim/evictionAlgo/Hyperbolic.h"
#include "../dataStructure/hashtable/hashtable.h"
#include <assert.h>


#ifdef __cplusplus
extern "C" {
#endif

cache_t *Hyperbolic_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("Hyperbolic", ccache_params);
  cache->cache_init = Hyperbolic_init;
  cache->cache_free = Hyperbolic_free;
  cache->get = Hyperbolic_get;
  cache->check = Hyperbolic_check;
  cache->insert = Hyperbolic_insert;
  cache->evict = Hyperbolic_evict;
  cache->remove = Hyperbolic_remove;

  return cache;
}

void Hyperbolic_free(cache_t *cache) { cache_struct_free(cache); }

cache_ck_res_e Hyperbolic_check(cache_t *cache, request_t *req, bool update_cache) {
  return cache_check(cache, req, update_cache, NULL);
}

cache_ck_res_e Hyperbolic_get(cache_t *cache, request_t *req) {
  return cache_get(cache, req);
}

void Hyperbolic_insert(cache_t *cache, request_t *req) {
  cache_insert_LRU(cache, req);
}

void Hyperbolic_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  cache_evict_LRU(cache, req, evicted_obj);
}


void Hyperbolic_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    ERROR("remove object %"PRIu64 "that is not cached\n", obj_id);
    return;
  }
  remove_obj_from_list(&cache->list_head, &cache->list_tail, obj);

  assert(cache->occupied_size >= obj->obj_size);
  cache->occupied_size -= (obj->obj_size + cache->per_obj_overhead);
  cache->n_obj -= 1;

  hashtable_delete(cache->hashtable, obj);
}

#ifdef __cplusplus
}
#endif
