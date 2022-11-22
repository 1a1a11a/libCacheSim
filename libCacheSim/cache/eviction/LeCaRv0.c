/* LeCaRv0: HotStorage'18
 *
 * the implementation uses a LRU and LFU, each object is stored both in LRU and
 * LFU, when updating, the object is updated in both caches when
 * evicting/removing, the object is removed from both
 *
 * this implementation is easier to understand, but it is not efficient due to
 * multiple hash table lookups
 *
 * */

#include "../../include/libCacheSim/evictionAlgo/LeCaRv0.h"

#include <math.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/LFU.h"
#include "../../include/libCacheSim/evictionAlgo/LRU.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LeCaRv0_params {
  cache_t *LRU;    // LRU
  cache_t *LRU_g;  // eviction history of LRU
  cache_t *LFU;    // LFU
  cache_t *LFU_g;  // eviction history of LFU
  double w_lru;
  double w_lfu;
  double lr;                 // learning rate
  double dr;                 // discount rate
  double ghost_list_factor;  // size(ghost_list)/size(cache), default 1
  int64_t n_hit_lru_history;
  int64_t n_hit_lfu_history;
} LeCaRv0_params_t;

cache_t *LeCaRv0_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LeCaRv0", ccache_params);
  cache->cache_init = LeCaRv0_init;
  cache->cache_free = LeCaRv0_free;
  cache->get = LeCaRv0_get;
  cache->check = LeCaRv0_check;
  cache->insert = LeCaRv0_insert;
  cache->evict = LeCaRv0_evict;
  cache->remove = LeCaRv0_remove;
  cache->to_evict = LeCaRv0_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->per_obj_metadata_size =
        8 * 2 + 8 * 2 + 8;  // LRU chain, LFU chain, history
  } else {
    cache->per_obj_metadata_size = 0;
  }

  if (cache_specific_params != NULL) {
    printf("LeCaRv0 does not support any parameters, but got %s\n",
           cache_specific_params);
    abort();
  }

  cache->eviction_params = my_malloc_n(LeCaRv0_params_t, 1);
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);
  params->ghost_list_factor = 1;
  params->lr = 0.45;
  params->dr = pow(0.005, 1.0 / (double)ccache_params.cache_size);
  params->w_lru = params->w_lfu = 0.50;
  params->n_hit_lru_history = params->n_hit_lfu_history = 0;

  params->LRU = LRU_init(ccache_params, NULL);
  params->LFU = LFU_init(ccache_params, NULL);

  common_cache_params_t ccache_params_g = ccache_params;
  /* set ghost_list_factor to 2 can reduce miss ratio anomaly */
  ccache_params_g.cache_size = (uint64_t)((double)ccache_params.cache_size / 2 *
                                          params->ghost_list_factor);

  params->LRU_g = LRU_init(ccache_params_g, NULL);
  params->LFU_g = LRU_init(ccache_params_g, NULL);

  return cache;
}

void LeCaRv0_free(cache_t *cache) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);
  params->LRU->cache_free(params->LRU);
  params->LRU_g->cache_free(params->LRU_g);
  params->LFU->cache_free(params->LFU);
  params->LFU_g->cache_free(params->LFU_g);
  my_free(sizeof(LeCaRv0_params_t), params);
  cache_struct_free(cache);
}

static void update_weight(cache_t *cache, int64_t t, double *w_update,
                          double *w_no_update) {
  DEBUG_ASSERT(fabs(*w_update + *w_no_update - 1.0) < 0.0001);
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);

  double r = pow(params->dr, (double)t);
  *w_update = *w_update * exp(-params->lr * r) + 1e-10; /* to avoid w was 0 */
  double s = *w_update + *w_no_update + +2e-10;
  *w_update = *w_update / s;
  *w_no_update = (*w_no_update + 1e-10) / s;
}

static void check_and_update_history(cache_t *cache, const request_t *req) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);

  cache_ck_res_e ck_lru_g, ck_lfu_g;

  ck_lru_g = params->LRU_g->check(params->LRU_g, req, false);
  ck_lfu_g = params->LFU_g->check(params->LFU_g, req, false);
  DEBUG_ASSERT((ck_lru_g == cache_ck_hit ? 1 : 0) +
                   (ck_lfu_g == cache_ck_hit ? 1 : 0) <=
               1);

  if (ck_lru_g == cache_ck_hit) {
    params->n_hit_lru_history++;
    cache_obj_t *obj = cache_get_obj(params->LRU_g, req);
    int64_t t = cache->n_req - obj->LeCaR.eviction_vtime;
    update_weight(cache, t, &params->w_lru, &params->w_lfu);
    params->LRU_g->remove(params->LRU_g, req->obj_id);
  } else if (ck_lfu_g == cache_ck_hit) {
    params->n_hit_lfu_history++;
    cache_obj_t *obj = cache_get_obj(params->LFU_g, req);
    int64_t t = cache->n_req - obj->LeCaR.eviction_vtime;
    update_weight(cache, t, &params->w_lfu, &params->w_lru);
    params->LFU_g->remove(params->LFU_g, req->obj_id);
  }
}

cache_ck_res_e LeCaRv0_check(cache_t *cache, const request_t *req,
                             bool update_cache) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);

  cache_ck_res_e ck_lru, ck_lfu;
  ck_lru = params->LRU->check(params->LRU, req, update_cache);
  ck_lfu = params->LFU->check(params->LFU, req, update_cache);
  DEBUG_ASSERT(ck_lru == ck_lfu);

  if (!update_cache) {
    return ck_lru;
  }

  if (ck_lru != cache_ck_hit) {
    /* cache miss */
    check_and_update_history(cache, req);
  }

  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);

  cache->occupied_size = params->LRU->occupied_size;

  return ck_lru;
}

cache_ck_res_e LeCaRv0_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_obj_t *LeCaRv0_insert(cache_t *cache, const request_t *req) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);

  params->LRU->insert(params->LRU, req);
  params->LFU->insert(params->LFU, req);

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj += 1;

  return NULL;
}

cache_obj_t *LeCaRv0_to_evict(cache_t *cache) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);
  double r = ((double)(next_rand() % 100)) / 100.0;
  if (r < params->w_lru) {
    return params->LRU->to_evict(params->LRU);
  } else {
    params->LFU->to_evict(params->LFU);
  }
}

void LeCaRv0_evict(cache_t *cache, const request_t *req,
                   cache_obj_t *evicted_obj) {
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);

  cache_obj_t obj;
  double r = ((double)(next_rand() % 100)) / 100.0;
  if (r < params->w_lru) {
    params->LRU->evict(params->LRU, req, &obj);
    params->LFU->remove(params->LFU, obj.obj_id);
    copy_cache_obj_to_request(req_local, &obj);
    DEBUG_ASSERT(params->LRU_g->check(params->LRU_g, req_local, false) ==
                 cache_ck_miss);
    if (req_local->obj_size < params->LRU_g->cache_size) {
      params->LRU_g->get(params->LRU_g, req_local);
      cache_get_obj(params->LRU_g, req_local)->LeCaR.eviction_vtime =
          cache->n_req;
    }
  } else {
    params->LFU->evict(params->LFU, req, &obj);
    params->LRU->remove(params->LRU, obj.obj_id);
    copy_cache_obj_to_request(req_local, &obj);
    DEBUG_ASSERT(params->LFU_g->check(params->LFU_g, req_local, false) ==
                 cache_ck_miss);
    if (req_local->obj_size < params->LFU_g->cache_size) {
      params->LFU_g->get(params->LFU_g, req_local);
      cache_get_obj(params->LFU_g, req_local)->LeCaR.eviction_vtime =
          cache->n_req;
    }
  }

  if (evicted_obj != NULL) {
    memcpy(evicted_obj, &obj, sizeof(cache_obj_t));
  }

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj -= 1;
}

void LeCaRv0_remove(cache_t *cache, obj_id_t obj_id) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_get_obj_by_id(params->LRU, obj_id);
  if (obj == NULL) {
    ERROR("remove object %" PRIu64 "that is not cached in LRU\n", obj_id);
  }
  params->LRU->remove(params->LRU, obj_id);

  obj = cache_get_obj_by_id(params->LFU, obj_id);
  if (obj == NULL) {
    ERROR("remove object %" PRIu64 "that is not cached in LFU\n", obj_id);
  }
  params->LFU->remove(params->LFU, obj_id);

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj -= 1;
}

#ifdef __cplusplus
}
#endif
