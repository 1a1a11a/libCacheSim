//
//  OptimalSize.c
//  libCacheSim
//
//  sample object and compare reuse_distance * size, then evict the greatest one
//
//

#include "../include/libCacheSim/evictionAlgo/OptimalSize.h"
#include "../dataStructure/hashtable/hashtable.h"


#ifdef __cplusplus
extern "C" {
#endif

#define N_SAMPLE_PER_EVICTION 32

cache_t *OptimalSize_init(common_cache_params_t ccache_params,
                     void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("OptimalSize", ccache_params);
  cache->cache_init = OptimalSize_init;
  cache->cache_free = OptimalSize_free;
  cache->get = OptimalSize_get;
  cache->check = OptimalSize_check;
  cache->insert = OptimalSize_insert;
  cache->evict = OptimalSize_evict;
  cache->remove = OptimalSize_remove;

  return cache;
}

void OptimalSize_free(cache_t *cache) { cache_struct_free(cache); }

cache_ck_res_e OptimalSize_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *obj;
  cache_ck_res_e ck = cache_check_base(cache, req, update_cache, &obj);

  if (update_cache && ck == cache_ck_hit) {
    if (req->next_access_ts == -1 || req->next_access_ts == INT64_MAX) {
      OptimalSize_remove(cache, obj->obj_id);
    } else {
      obj->optimal.next_access_ts = req->next_access_ts;
    }
  }

  return ck;
}

cache_ck_res_e OptimalSize_get(cache_t *cache, request_t *req) {
  return cache_get_base(cache, req);
}

void OptimalSize_insert(cache_t *cache, request_t *req) {
  if (req->next_access_ts == -1 || req->next_access_ts == INT64_MAX) {
    return;
  }

  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->optimal.next_access_ts = req->next_access_ts;
}

void OptimalSize_evict(cache_t *cache, request_t *req, cache_obj_t *cache_obj) {
  cache_obj_t *obj_to_evict, *sampled_obj;
  int64_t obj_to_evict_score = -1, sampled_obj_score;
  for (int i = 0; i < N_SAMPLE_PER_EVICTION; i++) {
    sampled_obj = hashtable_rand_obj(cache->hashtable);
    sampled_obj_score = sampled_obj->obj_size * (sampled_obj->optimal.next_access_ts - cache->vtime);
    if (obj_to_evict_score < sampled_obj_score) {
      obj_to_evict = sampled_obj;
      obj_to_evict_score = sampled_obj_score;
    }
  }

  if (cache_obj != NULL)
    memcpy(cache_obj, obj_to_evict, sizeof(cache_obj_t));
  cache_remove_obj_base(cache, obj_to_evict);
}

void OptimalSize_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);
  if (obj == NULL) {
    WARNING("obj to remove is not in the cache\n");
    return;
  }
  cache_remove_obj_base(cache, obj);
}

#ifdef __cplusplus
}
#endif
