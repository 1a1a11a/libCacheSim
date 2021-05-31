/* GDSF: greedy dual frequency size */

#include "libCacheSim/evictionAlgo/GDSF.h"
#include "hashtable.h"
#include <cassert>

#include "abstractRank.h"


namespace eviction {
class GDSF : virtual abstractRank {
public:
  GDSF() {};

  cache_ck_res_e GDSF_check(cache_t *cache, request_t *req, bool update_cache) {
    return cache_check_base(cache, req, update_cache, NULL);
  }

  cache_ck_res_e GDSF_get(cache_t *cache, request_t *req) {
    return cache_get_base(cache, req);
  }

  void GDSF_insert(cache_t *cache, request_t *req) {
    cache_insert_LRU(cache, req);
  }

  void GDSF_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
    cache_evict_LRU(cache, req, evicted_obj);
  }

  void GDSF_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
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


  void GDSF_remove(cache_t *cache, obj_id_t obj_id) {
    cache_obj_t *cache_obj = hashtable_find_obj_id(cache->hashtable, obj_id);
    if (cache_obj == NULL) {
      WARNING("obj is not in the cache\n");
      return;
    }
    remove_obj_from_list(&cache->list_head, &cache->list_tail, cache_obj);

    assert(cache->occupied_size >= cache_obj->obj_size);
    cache->occupied_size -= cache_obj->obj_size;

    hashtable_delete(cache->hashtable, cache_obj);
  }


};
}



cache_t *GDSF_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("GDSF", ccache_params);
  return cache;
}

void GDSF_free(cache_t *cache) { cache_struct_free(cache); }

cache_ck_res_e GDSF_check(cache_t *cache, request_t *req, bool update_cache) {
  return cache_check_base(cache, req, update_cache, NULL);
}

cache_ck_res_e GDSF_get(cache_t *cache, request_t *req) {
  return cache_get_base(cache, req);
}

void GDSF_insert(cache_t *cache, request_t *req) {
  cache_insert_LRU(cache, req);
}

void GDSF_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  cache_evict_LRU(cache, req, evicted_obj);
}

void GDSF_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
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


void GDSF_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *cache_obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (cache_obj == NULL) {
    WARNING("obj is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->list_head, &cache->list_tail, cache_obj);

  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= cache_obj->obj_size;

  hashtable_delete(cache->hashtable, cache_obj);
}