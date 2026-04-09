//
//  ClockOracle
//
//  An oracle-assisted CLOCK that combines the visited bit with oracle
//  reuse distance to decide reinsertion.
//
//  An object is reinserted only if BOTH conditions hold:
//    1. The visited bit is set (object was accessed since last eviction scan)
//    2. next_access_vtime - current_vtime <= cache_size / miss_ratio
//
//  The visited bit is cleared on reinsertion (like standard CLOCK).
//  Objects failing either condition are evicted.
//
//  Requires oracle traces (oracleGeneral / lcs) that provide next_access_vtime.
//
//  ClockOracle.c
//  libCacheSim
//

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void ClockOracle_free(cache_t *cache);
static bool ClockOracle_get(cache_t *cache, const request_t *req);
static cache_obj_t *ClockOracle_find(cache_t *cache, const request_t *req,
                                     bool update_cache);
static cache_obj_t *ClockOracle_insert(cache_t *cache, const request_t *req);
static cache_obj_t *ClockOracle_to_evict(cache_t *cache, const request_t *req);
static void ClockOracle_evict(cache_t *cache, const request_t *req);
static bool ClockOracle_remove(cache_t *cache, obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *ClockOracle_init(const common_cache_params_t ccache_params,
                          const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("ClockOracle", ccache_params, cache_specific_params);
  cache->cache_init = ClockOracle_init;
  cache->cache_free = ClockOracle_free;
  cache->get = ClockOracle_get;
  cache->find = ClockOracle_find;
  cache->insert = ClockOracle_insert;
  cache->evict = ClockOracle_evict;
  cache->remove = ClockOracle_remove;
  cache->can_insert = cache_can_insert_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->to_evict = ClockOracle_to_evict;
  cache->obj_md_size = 0;

  ClockOracle_params_t *params = my_malloc(ClockOracle_params_t);
  memset(params, 0, sizeof(ClockOracle_params_t));
  cache->eviction_params = params;

  params->q_head = NULL;
  params->q_tail = NULL;
  params->n_miss = 0;

  return cache;
}

static void ClockOracle_free(cache_t *cache) {
  my_free(sizeof(ClockOracle_params_t), cache->eviction_params);
  cache_struct_free(cache);
}

static bool ClockOracle_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

static cache_obj_t *ClockOracle_find(cache_t *cache, const request_t *req,
                                     bool update_cache) {
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);
  if (obj != NULL && update_cache) {
    /* set visited bit */
    obj->clock.freq = 1;
    obj->next_access_vtime = req->next_access_vtime;
  }
  return obj;
}

static cache_obj_t *ClockOracle_insert(cache_t *cache, const request_t *req) {
  ClockOracle_params_t *params =
      (ClockOracle_params_t *)cache->eviction_params;

  params->n_miss += 1;

  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);

  /* new objects start with visited bit clear */
  obj->clock.freq = 0;
  obj->next_access_vtime = req->next_access_vtime;

  return obj;
}

static cache_obj_t *ClockOracle_to_evict(cache_t *cache, const request_t *req) {
  ClockOracle_params_t *params =
      (ClockOracle_params_t *)cache->eviction_params;
  return params->q_tail;
}

/**
 * @brief evict using oracle + visited bit
 *
 * Scan from the tail. An object is reinserted only if BOTH:
 *   1. visited bit is set (freq >= 1)
 *   2. next_access_vtime - current_vtime <= cache_size / miss_ratio
 *
 * On reinsertion, the visited bit is cleared.
 * Objects failing either condition are evicted.
 */
static void ClockOracle_evict(cache_t *cache, const request_t *req) {
  ClockOracle_params_t *params =
      (ClockOracle_params_t *)cache->eviction_params;

  /* threshold = cache_size / miss_ratio = cache_size * n_req / n_miss
   * when n_miss == 0, use cache_size as the threshold */
  int64_t threshold;
  if (params->n_miss > 0) {
    threshold = (int64_t)((double)cache->cache_size * (double)cache->n_req /
                          (double)params->n_miss);
  } else {
    threshold = cache->cache_size;
  }


  cache_obj_t *obj_to_evict = params->q_tail;
  int64_t n_scanned = 0;
  while (obj_to_evict != NULL && n_scanned < cache->n_obj) {
    n_scanned++;

    bool no_future_access = (obj_to_evict->next_access_vtime == -1 ||
                             obj_to_evict->next_access_vtime == INT64_MAX);
    bool visited = (obj_to_evict->clock.freq >= 1);
    int64_t reuse_dist = obj_to_evict->next_access_vtime - cache->n_req;
    bool within_threshold = (!no_future_access && reuse_dist <= threshold);

    /* reinsert only if visited AND within threshold */
    if (visited && within_threshold) {
      /* clear visited bit, reinsert to head */
      obj_to_evict->clock.freq = 0;
      params->n_obj_rewritten += 1;
      params->n_byte_rewritten += obj_to_evict->obj_size;
      move_obj_to_head(&params->q_head, &params->q_tail, obj_to_evict);
      obj_to_evict = params->q_tail;
      continue;
    }

    /* evict: either not visited, no future access, or too far away */
    break;
  }

  /* safety: if everything was reinserted, evict the tail */
  if (obj_to_evict == NULL) {
    obj_to_evict = params->q_tail;
  }

  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_evict);
  cache_evict_base(cache, obj_to_evict, true);
}

static void ClockOracle_remove_obj(cache_t *cache, cache_obj_t *obj) {
  ClockOracle_params_t *params =
      (ClockOracle_params_t *)cache->eviction_params;

  DEBUG_ASSERT(obj != NULL);
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_remove_obj_base(cache, obj, true);
}

static bool ClockOracle_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  ClockOracle_remove_obj(cache, obj);

  return true;
}

#ifdef __cplusplus
}
#endif
