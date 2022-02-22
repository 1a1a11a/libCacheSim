/* LeCaR: HotStorage'18
 *
 * the implementation uses a LRU and LFU, each object is stored both in LRU and LFU,
 * when updating, the object is updated in both caches
 * when evicting/removing, the object is removed from both
 * */

#include "../include/libCacheSim/evictionAlgo/LeCaR.h"
#include "../dataStructure/hashtable/hashtable.h"
#include "../include/libCacheSim/evictionAlgo/LRU.h"
#include "../include/libCacheSim/evictionAlgo/LFU.h"
#include "../include/libCacheSim/evictionAlgo/LFUFast.h"

#ifdef __cplusplus
extern "C" {
#endif

cache_t *LeCaR_init(common_cache_params_t ccache_params_, void *init_params_) {
  cache_t *cache = cache_struct_init("LeCaR", ccache_params_);
  cache->cache_init = LeCaR_init;
  cache->cache_free = LeCaR_free;
  cache->get = LeCaR_get;
  cache->check = LeCaR_check;
  cache->insert = LeCaR_insert;
  cache->evict = LeCaR_evict;
  cache->remove = LeCaR_remove;
  cache->to_evict = LeCaR_to_evict;

  cache->eviction_params = my_malloc_n(LeCaR_params_t, 1);
  LeCaR_params_t *params = (LeCaR_params_t *) (cache->eviction_params);
  params->ghost_list_factor = 1;
  params->lr = 0.45;
  params->dr = pow(0.005, 1.0 / (double) ccache_params_.cache_size);
  params->w_lru = params->w_lfu = 0.50;
  params->n_hit_lru_history = params->n_hit_lfu_history = 0;

  params->LRU = LRU_init(ccache_params_, NULL);
  params->LFU = LFUFast_init(ccache_params_, NULL);

  /* set ghost_list_factor to 2 can reduce miss ratio anomaly */
  ccache_params_.cache_size = (uint64_t) (
      (double) ccache_params_.cache_size / 2 * params->ghost_list_factor);

  params->LRU_g = LRU_init(ccache_params_, NULL);
  params->LFU_g = LRU_init(ccache_params_, NULL);

  return cache;
}

void LeCaR_free(cache_t *cache) {
  LeCaR_params_t *params = (LeCaR_params_t *) (cache->eviction_params);
  params->LRU->cache_free(params->LRU);
  params->LRU_g->cache_free(params->LRU_g);
  params->LFU->cache_free(params->LFU);
  params->LFU_g->cache_free(params->LFU_g);
  my_free(sizeof(LeCaR_params_t), params);
  cache_struct_free(cache);
}

static void update_weight(cache_t *cache, int64_t t,
                          double *w_update, double *w_no_update) {
  DEBUG_ASSERT(fabs(*w_update + *w_no_update - 1.0) < 0.0001);
  LeCaR_params_t *params = (LeCaR_params_t *) (cache->eviction_params);

  double r = pow(params->dr, (double) t);
  *w_update = *w_update * exp(-params->lr * r) + 1e-10; /* to avoid w was 0 */
  double s = *w_update + *w_no_update +  + 2e-10;
  *w_update = *w_update / s;
  *w_no_update = (*w_no_update + 1e-10) / s;
}

static void check_and_update_history(cache_t *cache, request_t *req) {
  LeCaR_params_t *params = (LeCaR_params_t *) (cache->eviction_params);

  cache_ck_res_e ck_lru_g, ck_lfu_g;

  ck_lru_g = params->LRU_g->check(params->LRU_g, req, false);
  ck_lfu_g = params->LFU_g->check(params->LFU_g, req, false);
  DEBUG_ASSERT((ck_lru_g == cache_ck_hit ? 1 : 0) + (ck_lfu_g == cache_ck_hit ? 1 : 0) <= 1);

  if (ck_lru_g == cache_ck_hit) {
    params->n_hit_lru_history ++;
    cache_obj_t *obj = cache_get_obj(params->LRU_g, req);
    int64_t t = cache->n_req - obj->LeCaR.last_access_vtime;
    update_weight(cache, t, &params->w_lru, &params->w_lfu);
    params->LRU_g->remove(params->LRU_g, req->obj_id);
  } else if (ck_lfu_g == cache_ck_hit) {
      params->n_hit_lfu_history ++;
      cache_obj_t *obj = cache_get_obj(params->LFU_g, req);
      int64_t t = cache->n_req - obj->LeCaR.last_access_vtime;
      update_weight(cache, t, &params->w_lfu, &params->w_lru);
      params->LFU_g->remove(params->LFU_g, req->obj_id);
  }
}

cache_ck_res_e LeCaR_check(cache_t *cache, request_t *req, bool update_cache) {
  LeCaR_params_t *params = (LeCaR_params_t *) (cache->eviction_params);

//  static int64_t last_print;
//  if (params->vtime % 2000 == 0 && (
//      params->n_hit_lru_history + params->n_hit_lfu_history < 20 ||
//      params->n_hit_lru_history + params->n_hit_lfu_history > last_print)) {
//    last_print = params->n_hit_lru_history + params->n_hit_lfu_history;
//    printf("time %llu %lld %lld - %lf %lf\n",
//           params->vtime,
//           params->n_hit_lru_history, params->n_hit_lfu_history,
//           params->w_lru, params->w_lfu);
//  }

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

cache_ck_res_e LeCaR_get(cache_t *cache, request_t *req) {
  return cache_get_base(cache, req);
}

void LeCaR_insert(cache_t *cache, request_t *req) {
  LeCaR_params_t *params= (LeCaR_params_t *) (cache->eviction_params);

  params->LRU->insert(params->LRU, req);
  params->LFU->insert(params->LFU, req);

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj += 1;
}

cache_obj_t *LeCaR_to_evict(cache_t *cache) {
  LeCaR_params_t *params = (LeCaR_params_t *) (cache->eviction_params);
  double r = ((double) (next_rand() % 100)) / 100.0;
  if (r < params->w_lru) {
    return params->LRU->to_evict(params->LRU);
  } else {
    params->LFU->to_evict(params->LFU);
  }
}

void LeCaR_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  LeCaR_params_t *params = (LeCaR_params_t *) (cache->eviction_params);

  cache_obj_t obj;
  double r = ((double) (next_rand() % 100)) / 100.0;
  if (r < params->w_lru) {
    params->LRU->evict(params->LRU, req, &obj);
    params->LFU->remove(params->LFU, obj.obj_id);
    copy_cache_obj_to_request(req_local, &obj);
    DEBUG_ASSERT(params->LRU_g->check(params->LRU_g, req_local, false) == cache_ck_miss);
    params->LRU_g->get(params->LRU_g, req_local);
    cache_get_obj(params->LRU_g, req_local)->LeCaR.last_access_vtime = cache->n_req;
  } else {
    params->LFU->evict(params->LFU, req, &obj);
    params->LRU->remove(params->LRU, obj.obj_id);
    copy_cache_obj_to_request(req_local, &obj);
    DEBUG_ASSERT(params->LFU_g->check(params->LFU_g, req_local, false) == cache_ck_miss);
    params->LFU_g->get(params->LFU_g, req_local);
    cache_get_obj(params->LFU_g, req_local)->LeCaR.last_access_vtime = cache->n_req;
  }

  if (evicted_obj != NULL) {
    memcpy(evicted_obj, &obj, sizeof(cache_obj_t));
  }

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj -= 1;
}

void LeCaR_remove(cache_t *cache, obj_id_t obj_id) {
  LeCaR_params_t *params = (LeCaR_params_t *) (cache->eviction_params);
  cache_obj_t *obj = cache_get_obj_by_id(params->LRU, obj_id);
  if (obj == NULL) {
    ERROR("remove object %"PRIu64 "that is not cached in LRU\n", obj_id);
  }
  params->LRU->remove(params->LRU, obj_id);

  obj = cache_get_obj_by_id(params->LFU, obj_id);
  if (obj == NULL) {
    ERROR("remove object %"PRIu64 "that is not cached in LFU\n", obj_id);
  }
  params->LFU->remove(params->LFU, obj_id);

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj -= 1;
}


#ifdef __cplusplus
}
#endif
