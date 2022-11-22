/* Cacheus: FAST'21 */

#include "../../include/libCacheSim/evictionAlgo/Cacheus.h"

#include <assert.h>
#include <math.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/CR_LFU.h"
#include "../../include/libCacheSim/evictionAlgo/LRU.h"
#include "../../include/libCacheSim/evictionAlgo/SR_LRU.h"

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
} Cacheus_params_t;

cache_t *Cacheus_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params) {
  common_cache_params_t updated_cc_params = ccache_params;
  /* reduce the hash table size */
  updated_cc_params.hashpower -= 2;

  cache_t *cache = cache_struct_init("Cacheus", updated_cc_params);
  cache->cache_init = Cacheus_init;
  cache->cache_free = Cacheus_free;
  cache->get = Cacheus_get;
  cache->check = Cacheus_check;
  cache->insert = Cacheus_insert;
  cache->evict = Cacheus_evict;
  cache->remove = Cacheus_remove;
  cache->to_evict = Cacheus_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->per_obj_metadata_size =
        8 * 2 + 8 * 2 + 8;  // LRU chain, LFU chain, history
  } else {
    cache->per_obj_metadata_size = 0;
  }

  if (cache_specific_params != NULL) {
    printf("%s does not support any parameters, but got %s\n",
           cache->cache_name, cache_specific_params);
    abort();
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

void Cacheus_free(cache_t *cache) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  params->LRU->cache_free(params->LRU);
  params->LRU_g->cache_free(params->LRU_g);
  params->LFU->cache_free(params->LFU);
  params->LFU_g->cache_free(params->LFU_g);
  my_free(sizeof(Cacheus_params_t), params);
  cache_struct_free(cache);
}

static void update_weight(cache_t *cache, const request_t *req) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  cache_ck_res_e ck_lru_g, ck_lfu_g;

  ck_lru_g = params->LRU_g->check(params->LRU_g, req, false);
  ck_lfu_g = params->LFU_g->check(params->LFU_g, req, false);
  /* can only be evicted by one of the two experts, but is this true? (TODO) */
  DEBUG_ASSERT((ck_lru_g == cache_ck_hit ? 1 : 0) +
                   (ck_lfu_g == cache_ck_hit ? 1 : 0) <=
               1);

  if (ck_lru_g == cache_ck_hit) {
    params->w_lru = params->w_lru * exp(-params->lr);  // decrease weight_LRU
  } else if (ck_lfu_g == cache_ck_hit) {
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
    // Intuition: If hit rate is decreasing (deltla hit rate < 0)
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
    if (params->lr + sign * abs(params->lr * delta_lr) > 0.001)
      params->lr = params->lr + sign * abs(params->lr * delta_lr);
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

  cache_ck_res_e ck_lru_g, ck_lfu_g;

  ck_lru_g = params->LRU_g->check(params->LRU_g, req, false);
  ck_lfu_g = params->LFU_g->check(params->LFU_g, req, false);
  DEBUG_ASSERT((ck_lru_g == cache_ck_hit ? 1 : 0) +
                   (ck_lfu_g == cache_ck_hit ? 1 : 0) <=
               1);

  update_weight(cache, req);

  if (ck_lru_g == cache_ck_hit) {
    cache_obj_t *obj = cache_get_obj(params->LRU_g, req);
    params->LRU_g->remove(params->LRU_g, req->obj_id);
  } else if (ck_lfu_g == cache_ck_hit) {
    cache_obj_t *obj = cache_get_obj(params->LFU_g, req);
    params->LFU_g->remove(params->LFU_g, req->obj_id);
  }
}

cache_ck_res_e Cacheus_check(cache_t *cache, const request_t *req,
                             bool update_cache) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);

  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);

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
  } else {
    // If hit, update the data structure and increment num_hit counter
    params->num_hit += 1;
  }

  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);

  cache->occupied_size = params->LRU->occupied_size;
  return ck_lru;
}

cache_ck_res_e Cacheus_get(cache_t *cache, const request_t *req) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);
  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->occupied_size == cache->occupied_size);

  cache->n_req += 1;
  cache_ck_res_e ret = cache->check(cache, req, true);

  if (ret != cache_ck_hit) {
    SR_LRU_params_t *params_LRU =
        (SR_LRU_params_t *)(params->LRU->eviction_params);
    if (req->obj_size + cache->per_obj_metadata_size > cache->cache_size ||
        req->obj_size + cache->per_obj_metadata_size >
            params_LRU->SR_list->cache_size) {
      static __thread bool has_printed = false;
      if (!has_printed) {
        has_printed = true;
        if (req->obj_size + cache->per_obj_metadata_size > cache->cache_size) {
          WARN("req %" PRIu64 ": obj size %" PRIu32
               " larger than cache size %" PRIu64 "\n",
               req->obj_id, req->obj_size, cache->cache_size);
        } else if (req->obj_size + cache->per_obj_metadata_size >
                   params_LRU->SR_list->cache_size) {
          WARN("req %" PRIu64 ": obj size %" PRIu32
               " larger than SR-LRU - SR size %" PRIu64 "\n",
               req->obj_id, req->obj_size, params_LRU->SR_list->cache_size);
        }
      }
    }

    else if (ret == cache_ck_miss) {
      while (cache->occupied_size + req->obj_size + cache->per_obj_metadata_size >
             cache->cache_size)
        cache->evict(cache, req, NULL);

      cache->insert(cache, req);
    }
  }
  // cache_ck_res_e ret = cache_get_base(cache, req);

  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);

  if (cache->n_req % params->update_interval == 0) update_lr(cache, req);
  return ret;
}

cache_obj_t *Cacheus_insert(cache_t *cache, const request_t *req) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);

  // LFU must be first because it may load frequency stored in lRU history
  params->LFU->insert(params->LFU, req);
  params->LRU->insert(params->LRU, req);

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj = params->LRU->n_obj;
  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == params->LFU->n_obj);

  /* the cached obj is stored twice */
  return NULL;
}

cache_obj_t *Cacheus_to_evict(cache_t *cache) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  double r = ((double)(next_rand() % 100)) / 100.0;
  if (r < params->w_lru) {
    return params->LRU->to_evict(params->LRU);
  } else {
    return params->LFU->to_evict(params->LFU);
  }
}

void Cacheus_evict(cache_t *cache, const request_t *req,
                   cache_obj_t *evicted_obj) {
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == cache->n_obj);
  cache_obj_t obj;
  double r = ((double)(next_rand() % 100)) / 100.0;

  // If two voters decide the same:
  if (params->LRU->to_evict(params->LRU)->obj_id ==
      params->LFU->to_evict(params->LFU)->obj_id) {
    params->LRU->evict(params->LRU, req, &obj);
    params->LFU->evict(params->LFU, req, &obj);
  } else if (r < params->w_lru) {
    params->LRU->evict(params->LRU, req, &obj);
    params->LFU->remove(params->LFU, obj.obj_id);
    copy_cache_obj_to_request(req_local, &obj);
    DEBUG_ASSERT(params->LRU_g->check(params->LRU_g, req_local, false) ==
                 cache_ck_miss);
    if (req_local->obj_size < params->LRU_g->cache_size) {
      params->LRU_g->get(params->LRU_g, req_local);
    }

    // action is LFU
  } else {
    // Remove first because LFU needs to offload the freq to obj in LRU history
    params->LRU->remove(params->LRU,
                        params->LFU->to_evict(params->LFU)->obj_id);
    params->LFU->evict(params->LFU, req, &obj);
    copy_cache_obj_to_request(req_local, &obj);
    DEBUG_ASSERT(params->LFU_g->check(params->LFU_g, req_local, false) ==
                 cache_ck_miss);
    if (req_local->obj_size < params->LRU_g->cache_size) {
      params->LFU_g->get(params->LFU_g, req_local);
    }
  }

  if (evicted_obj != NULL) {
    memcpy(evicted_obj, &obj, sizeof(cache_obj_t));
  }

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj = params->LRU->n_obj;
  DEBUG_ASSERT(params->LRU->occupied_size == params->LFU->occupied_size);
  DEBUG_ASSERT(params->LRU->n_obj == params->LFU->n_obj);
}

void Cacheus_remove(cache_t *cache, const obj_id_t obj_id) {
  Cacheus_params_t *params = (Cacheus_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_get_obj_by_id(params->LRU, obj_id);
  if (obj == NULL) {
    PRINT_ONCE("remove object %" PRIu64 "that is not cached in LRU\n", obj_id);
  }
  params->LRU->remove(params->LRU, obj_id);

  obj = cache_get_obj_by_id(params->LFU, obj_id);
  if (obj == NULL) {
    PRINT_ONCE("remove object %" PRIu64 "that is not cached in LFU\n", obj_id);
  }
  params->LFU->remove(params->LFU, obj_id);

  cache->occupied_size = params->LRU->occupied_size;
  cache->n_obj -= 1;
}

#ifdef __cplusplus
}
#endif
