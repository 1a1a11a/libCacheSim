/* Cacheus: FAST'21 */

#include "../../include/libCacheSim/evictionAlgo/Cacheus.h"

#include <assert.h>
#include <math.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Cacheus_params {
  cache_t *LRU;        // LRU
  cache_t *LRU_g;      // eviction history of LRU
  cache_t *LFU;        // LFU
  cache_t *LFU_g;      // eviction history of LFU
  double w_lru;        // Weight for LRU
  double w_lfu;        // Weight for LFU
  double lr;           // learning rate
  double lr_previous;  // previous learning rate

  double ghost_list_factor;  // size(ghost_list)/size(cache), default 1
  int64_t unlearn_count;

  int64_t num_hit;
  double hit_rate_prev;

  uint64_t update_interval;
  request_t *req_local;
} Cacheus_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void Cacheus_free(cache_t *cache);
static bool Cacheus_get(cache_t *cache, const request_t *req);
static cache_obj_t *Cacheus_find(cache_t *cache, const request_t *req,
                                 const bool update_cache);
static cache_obj_t *Cacheus_insert(cache_t *cache, const request_t *req);
static cache_obj_t *Cacheus_to_evict(cache_t *cache, const request_t *req);
static void Cacheus_evict(cache_t *cache, const request_t *req);
static bool Cacheus_remove(cache_t *cache, const obj_id_t obj_id);

static inline int64_t Cacheus_get_occupied_byte(const cache_t *cache);
static inline int64_t Cacheus_get_n_obj(const cache_t *cache);

/* internal functions */
static void update_weight(cache_t *cache, const request_t *req);
static void update_lr(cache_t *cache, const request_t *req);
static void check_and_update_history(cache_t *cache, const request_t *req);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize a Cacheus cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params Cacheus specific parameters, should be NULL
 */
cache_t *Cacheus_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params) {
  common_cache_params_t updated_cc_params = ccache_params;
  /* reduce the hash table size */
  updated_cc_params.hashpower -= 2;

  cache_t *cache = cache_struct_init("Cacheus", updated_cc_params, cache_specific_params);
  cache->cache_init = Cacheus_init;
  cache->cache_free = Cacheus_free;
  cache->get = Cacheus_get;
  cache->find = Cacheus_find;
  cache->insert = Cacheus_insert;
  cache->evict = Cacheus_evict;
  cache->remove = Cacheus_remove;
  cache->to_evict = Cacheus_to_evict;
  cache->can_insert = cache_can_insert_default;
  cache->get_n_obj = Cacheus_get_n_obj;
  cache->get_occupied_byte = Cacheus_get_occupied_byte;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2 + 8 * 2 + 8;  // LRU chain, LFU chain, history
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc_n(Cacheus_params_t, 1);
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  params->ghost_list_factor = 1;
  params->update_interval = ccache_params.cache_size;  // From paper
  // learning rate chooses randomly between 10-3 & 1
  // LR will be reset after 10 consecutive decreases. Whether reset to the same
  // val or diff value, the repo differs from their paper. I followed paper.
  params->lr = 0.001 + ((double)(next_rand() % 1000)) / 1000;
  params->lr_previous = 0;

  params->w_lru = params->w_lfu = 0.50;  // weights for LRU and LFU
  params->num_hit = 0;
  params->hit_rate_prev = 0;
  params->req_local = new_request();

  params->LRU = SR_LRU_init(ccache_params, NULL);
  params->LFU = CR_LFU_init(ccache_params, NULL);
  SR_LRU_params_t *SR_LRU_params =
      (SR_LRU_params_t *)(params->LRU->eviction_params);
  // When SR_LRU evicts, makes CR_LFU evicts as well.
  SR_LRU_params->other_cache = params->LFU;
  CR_LFU_params_t *CR_LFU_params =
      (CR_LFU_params_t *)(params->LFU->eviction_params);
  CR_LFU_params->other_cache = params->LRU;

  common_cache_params_t ccache_params_g = updated_cc_params;
  /* set ghost_list_factor to 2 can reduce miss ratio anomaly */
  ccache_params_g.cache_size = (uint64_t)((double)ccache_params.cache_size / 2 *
                                          params->ghost_list_factor);

  params->LRU_g = LRU_init(ccache_params_g, NULL);  // LRU_history
  params->LFU_g = LRU_init(ccache_params_g, NULL);  // LFU_history
  return cache;
}

static void Cacheus_free(cache_t *cache) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  free_request(params->req_local);
  params->LRU->cache_free(params->LRU);
  params->LRU_g->cache_free(params->LRU_g);
  params->LFU->cache_free(params->LFU);
  params->LFU_g->cache_free(params->LFU_g);
  my_free(sizeof(Cacheus_params_t), params);
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
static bool Cacheus_get(cache_t *cache, const request_t *req) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  cache_t *lru = params->LRU;
  cache_t *lfu = params->LFU;

  lru->n_req = lfu->n_req = cache->n_req + 1;
  bool ret = cache_get_base(cache, req);

  if (cache->n_req % params->update_interval == 0) {
    update_lr(cache, req);
  }

  DEBUG_ASSERT(lru->get_occupied_byte(lru) == lfu->get_occupied_byte(lfu));
  DEBUG_ASSERT(lru->get_n_obj(lru) == lfu->get_n_obj(lfu));

  return ret;
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief check whether an object is in the cache
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return true on hit, false on miss
 */
static cache_obj_t *Cacheus_find(cache_t *cache, const request_t *req,
                                 bool update_cache) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);

  cache_obj_t *obj_lru, *obj_lfu;
  obj_lru = params->LRU->find(params->LRU, req, update_cache);
  obj_lfu = params->LFU->find(params->LFU, req, update_cache);
  bool hit_lru = obj_lru != NULL;
  bool hit_lfu = obj_lfu != NULL;
  DEBUG_ASSERT((hit_lru == hit_lfu));

  if (!update_cache) {
    // TODO: this is weird because the object is in two caches
    return obj_lru;
  }

  if (!hit_lru) {
    /* cache miss */
    check_and_update_history(cache, req);
  } else {
    // If hit, update the data structure and increment num_hit counter
    params->num_hit += 1;
  }

  // TODO: this is weird because the object is in two caches
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
static cache_obj_t *Cacheus_insert(cache_t *cache, const request_t *req) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);

  // LFU must be first because it may load frequency stored in LRU history
  if (!params->LRU->can_insert(params->LRU, req)) {
    return NULL;
  }
  params->LFU->insert(params->LFU, req);
  params->LRU->insert(params->LRU, req);

  /* the cached obj is stored twice, so we cannot really return an object */
  return NULL;
}

// TODO(jason): this function may not work
static cache_obj_t *Cacheus_to_evict(cache_t *cache, const request_t *req) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  // update the candidate gen time
  cache->to_evict_candidate_gen_vtime = cache->n_req;

  double r = ((double)(next_rand() % 100)) / 100.0;
  if (r < params->w_lru) {
    cache->to_evict_candidate = params->LRU->to_evict(params->LRU, req);
  } else {
    cache->to_evict_candidate = params->LFU->to_evict(params->LFU, req);
  }
  return cache->to_evict_candidate;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 */
static void Cacheus_evict(cache_t *cache, const request_t *req) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  cache_t *lru = params->LRU;
  cache_t *lfu = params->LFU;
  cache_t *lru_g = params->LRU_g;
  cache_t *lfu_g = params->LFU_g;

  // If two voters decide the same:
  cache_obj_t *lru_to_evict = lru->to_evict(lru, req);
  cache_obj_t *lfu_to_evict = lfu->to_evict(lfu, req);
  DEBUG_ASSERT(lru_to_evict != NULL);
  DEBUG_ASSERT(lfu_to_evict != NULL);

  cache_obj_t *obj_to_evict = NULL;
  if (cache->to_evict_candidate_gen_vtime == cache->n_req) {
    obj_to_evict = cache->to_evict_candidate;
  } else {
    obj_to_evict = Cacheus_to_evict(cache, req);
  }

  if (lru_to_evict->obj_id == lfu_to_evict->obj_id) {
    lru->evict(lru, req);
    lfu->evict(lfu, req);
  } else if (obj_to_evict == lru_to_evict) {
    copy_cache_obj_to_request(params->req_local, obj_to_evict);
    lru->evict(lru, req);
    bool removed = lfu->remove(lfu, params->req_local->obj_id);
    DEBUG_ASSERT(removed);
    // insert into ghost
    bool ghost_hit = lru_g->get(lru_g, params->req_local);
    DEBUG_ASSERT(!ghost_hit);
  } else {
    // Remove first because LFU needs to offload the freq to obj in LRU
    // history
    copy_cache_obj_to_request(params->req_local, obj_to_evict);
    // LRU remove needs to before LFU evict
    bool removed = lru->remove(lru, params->req_local->obj_id);
    DEBUG_ASSERT(removed);
    lfu->evict(lfu, req);
    // insert into ghost
    bool ghost_hit = lfu_g->get(lfu_g, params->req_local);
    DEBUG_ASSERT(!ghost_hit);
  }

  cache->to_evict_candidate_gen_vtime = -1;
}

static bool Cacheus_remove(cache_t *cache, const obj_id_t obj_id) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  params->req_local->obj_id = obj_id;
  bool lru_removed = params->LRU->remove(params->LRU, obj_id);
  bool lfu_removed = params->LFU->remove(params->LFU, obj_id);
  DEBUG_ASSERT(lru_removed == lfu_removed);
  return lru_removed;
}

static inline int64_t Cacheus_get_occupied_byte(const cache_t *cache) {
  Cacheus_params_t *params = (Cacheus_params_t *)cache->eviction_params;
  int64_t occupied_byte = params->LRU->get_occupied_byte(params->LRU);
  DEBUG_ASSERT(occupied_byte == params->LFU->get_occupied_byte(params->LFU));
  return occupied_byte;
}

static inline int64_t Cacheus_get_n_obj(const cache_t *cache) {
  Cacheus_params_t *params = (Cacheus_params_t *)cache->eviction_params;
  int64_t n_obj = params->LRU->get_n_obj(params->LRU);
  DEBUG_ASSERT(n_obj == params->LFU->get_n_obj(params->LFU));
  return n_obj;
}

// ***********************************************************************
// ****                                                               ****
// ****                    internal functions                         ****
// ****                                                               ****
// ***********************************************************************

static void update_weight(cache_t *cache, const request_t *req) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  bool cache_hit_lru_g, cache_hit_lfu_g;

  cache_hit_lru_g = params->LRU_g->find(params->LRU_g, req, false) != NULL;
  cache_hit_lfu_g = params->LFU_g->find(params->LFU_g, req, false) != NULL;
  /* can only be evicted by one of the two experts, but is this true? (TODO) */
  DEBUG_ASSERT((cache_hit_lru_g ? 1 : 0) + (cache_hit_lfu_g ? 1 : 0) <= 1);

  if (cache_hit_lru_g) {
    params->w_lru = params->w_lru * exp(-params->lr);  // decrease weight_LRU
  } else if (cache_hit_lfu_g) {
    params->w_lfu = params->w_lfu * exp(-params->lr);  // decrease weight_LFU
  }
  // normalize
  params->w_lru = params->w_lru / (params->w_lru + params->w_lfu);
  params->w_lfu = 1 - params->w_lru;
}

static void update_lr(cache_t *cache, const request_t *req) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  // Here: num_hit is the number of hits; reset to 0 when update_lr is called.
  double hit_rate_current = params->num_hit / params->update_interval;
  double delta_hit_rate = hit_rate_current - params->hit_rate_prev;
  double delta_lr = params->lr - params->lr_previous;

  params->lr_previous = params->lr;
  params->hit_rate_prev = hit_rate_current;

  if (delta_lr != 0) {
    int sign;
    // Intuition: If hit rate is decreasing (delta hit rate < 0)
    // Learning rate is positive (delta_lr > 0)
    // sign = -1 => decrease learning rate;
    if (delta_hit_rate / delta_lr > 0)
      sign = 1;
    else
      sign = -1;
    // If hit rate > learning rate
    // sign positive, increase learning rate;

    // Repo has this part: not included in the original paper.
    // if self.learning_rate >= 1:
    // self.learning_rate = 0.9
    // elif self.learning_rate <= 0.001:
    // self.learning_rate = 0.005
    if (params->lr + sign * fabs(params->lr * delta_lr) > 0.001)
      params->lr = params->lr + sign * fabs(params->lr * delta_lr);
    else
      params->lr = 0.0001;
    params->unlearn_count = 0;
  } else {
    if (hit_rate_current == 0 || delta_hit_rate <= 0)
      params->unlearn_count += 1;
    else if (params->unlearn_count >= 10) {
      params->unlearn_count = 0;
      params->lr =
          0.001 + ((double)(next_rand() % 1000)) /
                      1000;  // learning rate chooses randomly between 10-3 & 1
    }
  }
  params->num_hit = 0;
}

// Update weight and history, only called in cache miss.
static void check_and_update_history(cache_t *cache, const request_t *req) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);

  bool hit_lru_g = params->LRU_g->find(params->LRU_g, req, false) != NULL;
  bool hit_lfu_g = params->LFU_g->find(params->LFU_g, req, false) != NULL;
  DEBUG_ASSERT((hit_lru_g ? 1 : 0) + (hit_lfu_g ? 1 : 0) <= 1);

  update_weight(cache, req);

  params->LRU_g->remove(params->LRU_g, req->obj_id);
  params->LFU_g->remove(params->LFU_g, req->obj_id);
}

#ifdef __cplusplus
}
#endif
