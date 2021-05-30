/* LeCaR: HotStorage'18 */

#include "../include/libCacheSim/evictionAlgo/LeCaR.h"
#include "../dataStructure/hashtable/hashtable.h"
#include <assert.h>


#ifdef __cplusplus
extern "C" {
#endif

cache_t *LeCaR_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("LeCaR", ccache_params);
  cache->cache_init = LeCaR_init;
  cache->cache_free = LeCaR_free;
  cache->get = LeCaR_get;
  cache->check = LeCaR_check;
  cache->insert = LeCaR_insert;
  cache->evict = LeCaR_evict;
  cache->remove = LeCaR_remove;

  return cache;
}

void LeCaR_free(cache_t *cache) { cache_struct_free(cache); }

cache_ck_res_e LeCaR_check(cache_t *cache, request_t *req, bool update_cache) {
  return cache_check(cache, req, update_cache, NULL);
}

cache_ck_res_e LeCaR_get(cache_t *cache, request_t *req) {
  return cache_get(cache, req);
}

void LeCaR_insert(cache_t *cache, request_t *req) {
  cache_insert_LRU(cache, req);
}

void LeCaR_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  cache_evict_LRU(cache, req, evicted_obj);
}

void LeCaR_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  cache_obj_t *cache_obj = hashtable_find_obj(cache->hashtable, obj_to_remove);
  if (cache_obj == NULL) {
    WARNING("obj is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->list_head, &cache->list_tail,
                       cache_obj);
  hashtable_delete(cache->hashtable, cache_obj);

  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= cache_obj->obj_size;
}


void LeCaR_remove(cache_t *cache, obj_id_t obj_id) {
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
