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

#include <math.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

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

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void LeCaRv0_free(cache_t *cache);
static bool LeCaRv0_get(cache_t *cache, const request_t *req);
static cache_obj_t *LeCaRv0_find(cache_t *cache, const request_t *req,
                                 const bool update_cache);
static cache_obj_t *LeCaRv0_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LeCaRv0_to_evict(cache_t *cache, const request_t *req);
static void LeCaRv0_evict(cache_t *cache, const request_t *req);
static bool LeCaRv0_remove(cache_t *cache, const obj_id_t obj_id);

/* internal functions */
static void update_weight(cache_t *cache, int64_t t, double *w_update,
                          double *w_no_update);
static void check_and_update_history(cache_t *cache, const request_t *req);

static inline int64_t LeCaRv0_get_n_obj(const cache_t *cache) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);
  DEBUG_ASSERT(params->LRU->get_n_obj(params->LRU) ==
               params->LFU->get_n_obj(params->LFU));
  return params->LRU->get_n_obj(params->LRU);
}

static inline int64_t LeCaRv0_get_occupied_byte(const cache_t *cache) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);
  DEBUG_ASSERT(params->LRU->get_occupied_byte(params->LRU) ==
               params->LFU->get_occupied_byte(params->LFU));
  return params->LRU->get_occupied_byte(params->LRU);
}
// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************
/**
 * @brief initialize the cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, should be NULL
 */
cache_t *LeCaRv0_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LeCaRv0", ccache_params, cache_specific_params);
  cache->cache_init = LeCaRv0_init;
  cache->cache_free = LeCaRv0_free;
  cache->get = LeCaRv0_get;
  cache->find = LeCaRv0_find;
  cache->insert = LeCaRv0_insert;
  cache->evict = LeCaRv0_evict;
  cache->remove = LeCaRv0_remove;
  cache->to_evict = LeCaRv0_to_evict;
  cache->get_n_obj = LeCaRv0_get_n_obj;
  cache->get_occupied_byte = LeCaRv0_get_occupied_byte;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2 + 8 * 2 + 8;  // LRU chain, LFU chain, history
  } else {
    cache->obj_md_size = 0;
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

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void LeCaRv0_free(cache_t *cache) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);
  params->LRU->cache_free(params->LRU);
  params->LRU_g->cache_free(params->LRU_g);
  params->LFU->cache_free(params->LFU);
  params->LFU_g->cache_free(params->LFU_g);
  my_free(sizeof(LeCaRv0_params_t), params);
  cache_struct_free(cache);
}

/**
 * @brief this function is the user facing API
 * it performs the following logic
 *
 * ```
 * if obj in cache:
 *    update_metadata
 *    return true
 * else:
 *    if cache does not have enough space:
 *        evict until it has space to insert
 *    insert the object
 *    return false
 * ```
 *
 * @param cache
 * @param req
 * @return true if cache hit, false if cache miss
 */

static bool LeCaRv0_get(cache_t *cache, const request_t *req) {
  /* occupied bytes and n_obj are maintained by LRU and LFU
   * see get_n_obj and get_occupied_byte */
  DEBUG_ASSERT(cache->occupied_byte == 0);

  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief find an object in the cache
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return the object or NULL if not found
 */
static cache_obj_t *LeCaRv0_find(cache_t *cache, const request_t *req,
                                 bool update_cache) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);

  cache_obj_t *obj_lru, *obj_lfu;
  obj_lru = params->LRU->find(params->LRU, req, update_cache);
  obj_lfu = params->LFU->find(params->LFU, req, update_cache);
  DEBUG_ASSERT((obj_lru == NULL && NULL == obj_lfu) ||
               (obj_lru != NULL && NULL != obj_lfu));

  if (!update_cache) {
    return obj_lru;
  }

  if (obj_lru == NULL) {
    /* cache miss */
    check_and_update_history(cache, req);
  }

  return obj_lru;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * and eviction is not part of this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
cache_obj_t *LeCaRv0_insert(cache_t *cache, const request_t *req) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);

  params->LRU->insert(params->LRU, req);
  params->LFU->insert(params->LFU, req);

  cache->n_obj += 1;

  return NULL;
}

/**
 * @brief find the object to be evicted
 * this function does not actually evict the object or update metadata
 * not all eviction algorithms support this function
 * because the eviction logic cannot be decoupled from finding eviction
 * candidate, so use assert(false) if you cannot support this function
 *
 * @param cache the cache
 * @return the object to be evicted
 */
static cache_obj_t *LeCaRv0_to_evict(cache_t *cache, const request_t *req) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);
  double r = ((double)(next_rand() % 100)) / 100.0;
  if (r < params->w_lru) {
    return params->LRU->to_evict(params->LRU, req);
  } else {
    params->LFU->to_evict(params->LFU, req);
  }
  return NULL;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 */
static void LeCaRv0_evict(cache_t *cache, const request_t *req) {
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);

  cache_obj_t *lru_candidate = params->LRU->to_evict(params->LRU, req);
  cache_obj_t *lfu_candidate = params->LFU->to_evict(params->LFU, req);

  if (lru_candidate->obj_id == lfu_candidate->obj_id) {
    copy_cache_obj_to_request(req_local, lru_candidate);
    params->LFU->remove(params->LFU, lru_candidate->obj_id);
    params->LRU->evict(params->LRU, req);
  } else {
    double r = ((double)(next_rand() % 100)) / 100.0;
    if (r < params->w_lru) {
      copy_cache_obj_to_request(req_local, lru_candidate);
      params->LFU->remove(params->LFU, lru_candidate->obj_id);
      params->LRU->evict(params->LRU, req);
      DEBUG_ASSERT(!params->LRU_g->find(params->LRU_g, req_local, false));
      params->LRU_g->get(params->LRU_g, req_local);
      cache_obj_t *obj = params->LRU_g->find(params->LRU_g, req_local, false);
      obj->LeCaR.eviction_vtime = cache->n_req;
    } else {
      copy_cache_obj_to_request(req_local, lfu_candidate);
      params->LRU->remove(params->LRU, lfu_candidate->obj_id);
      params->LFU->evict(params->LFU, req);
      DEBUG_ASSERT(!params->LFU_g->find(params->LFU_g, req_local, false));
      params->LFU_g->get(params->LFU_g, req_local);
      cache_obj_t *obj = params->LFU_g->find(params->LFU_g, req_local, false);
      obj->LeCaR.eviction_vtime = cache->n_req;
    }
  }
}

/**
 * @brief remove an object from the cache
 * this is different from cache_evict because it is used to for user trigger
 * remove, and eviction is used by the cache to make space for new objects
 *
 * it needs to call cache_remove_obj_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param obj_id
 * @return true if the object is removed, false if the object is not in the
 * cache
 */
static bool LeCaRv0_remove(cache_t *cache, obj_id_t obj_id) {
  LeCaRv0_params_t *params = (LeCaRv0_params_t *)(cache->eviction_params);
  cache_obj_t *obj = hashtable_find_obj_id(params->LRU->hashtable, obj_id);
  if (obj == NULL) {
    ERROR("remove object %" PRIu64 "that is not cached in LRU\n", obj_id);
  }
  params->LRU->remove(params->LRU, obj_id);

  obj = hashtable_find_obj_id(params->LFU->hashtable, obj_id);
  if (obj == NULL) {
    ERROR("remove object %" PRIu64 "that is not cached in LFU\n", obj_id);
    return false;
  }
  params->LFU->remove(params->LFU, obj_id);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                         internal functions                    ****
// ****                                                               ****
// ***********************************************************************
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

  bool cache_hit_lru_g, cache_hit_lfu_g;

  cache_hit_lru_g = params->LRU_g->find(params->LRU_g, req, false) != NULL;
  cache_hit_lfu_g = params->LFU_g->find(params->LFU_g, req, false) != NULL;
  DEBUG_ASSERT((cache_hit_lru_g ? 1 : 0) + (cache_hit_lfu_g ? 1 : 0) <= 1);

  if (cache_hit_lru_g) {
    params->n_hit_lru_history++;
    cache_obj_t *obj = hashtable_find(params->LRU_g->hashtable, req);
    int64_t t = cache->n_req - obj->LeCaR.eviction_vtime;
    update_weight(cache, t, &params->w_lru, &params->w_lfu);
    params->LRU_g->remove(params->LRU_g, req->obj_id);
  } else if (cache_hit_lfu_g) {
    params->n_hit_lfu_history++;
    cache_obj_t *obj = hashtable_find(params->LFU_g->hashtable, req);
    int64_t t = cache->n_req - obj->LeCaR.eviction_vtime;
    update_weight(cache, t, &params->w_lfu, &params->w_lru);
    params->LFU_g->remove(params->LFU_g, req->obj_id);
  }
}

#ifdef __cplusplus
}
#endif
