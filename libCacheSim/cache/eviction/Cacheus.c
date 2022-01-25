/* Cacheus: FAST'21 */

#include "../include/libCacheSim/evictionAlgo/Cacheus.h"
#include "../dataStructure/hashtable/hashtable.h"
#include <assert.h>


#ifdef __cplusplus
extern "C" {
#endif

cache_t *Cacheus_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("Cacheus", ccache_params);
  return cache;
}

void Cacheus_free(cache_t *cache) { cache_struct_free(cache); }

cache_ck_res_e Cacheus_check(cache_t *cache, request_t *req, bool update_cache) {
  return cache_check_base(cache, req, update_cache, NULL);
}

cache_ck_res_e Cacheus_get(cache_t *cache, request_t *req) {
  return cache_get_base(cache, req);
}

void Cacheus_insert(cache_t *cache, request_t *req) {
  cache_insert_LRU(cache, req);
}

void Cacheus_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  cache_evict_LRU(cache, req, evicted_obj);
}

void Cacheus_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *cache_obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (cache_obj == NULL) {
    WARN("obj is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->q_head, &cache->q_tail, cache_obj);

  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_overhead);
  cache->n_obj -= 1;

  hashtable_delete(cache->hashtable, cache_obj);
}

#ifdef __cplusplus
}
#endif
