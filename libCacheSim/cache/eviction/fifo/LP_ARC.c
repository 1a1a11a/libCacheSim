//
//  LP_ARC cache replacement algorithm
//  https://www.usenix.org/conference/fast-03/LP_ARC-self-tuning-low-overhead-replacement-cache
//
//
//  the LRU queues are replaced with FIFO or others
//
//
//  libCacheSim
//
//  Created by Juncheng on 09/28/20.
//  Copyright © 2020 Juncheng. All rights reserved.
//

#include <string.h>

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LP_ARC_params {
  // L1_data is T1 in the paper, L1_ghost is B1 in the paper
  cache_t *T1;
  cache_t *B1;
  cache_t *T2;
  cache_t *B2;

  double p;
  bool curr_obj_in_L1_ghost;
  bool curr_obj_in_L2_ghost;
  int64_t vtime_last_req_in_ghost;
  request_t *req_local;
} LP_ARC_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void LP_ARC_parse_params(cache_t *cache,
                                const char *cache_specific_params);
static void LP_ARC_free(cache_t *cache);
static bool LP_ARC_get(cache_t *cache, const request_t *req);
static cache_obj_t *LP_ARC_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *LP_ARC_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LP_ARC_to_evict(cache_t *cache, const request_t *req);
static void LP_ARC_evict(cache_t *cache, const request_t *req);
static bool LP_ARC_remove(cache_t *cache, const obj_id_t obj_id);
static int64_t LP_ARC_get_occupied_byte(const cache_t *cache);
static int64_t LP_ARC_get_n_obj(const cache_t *cache);

/* internal functions */

/* this is the case IV in the paper */
static void _LP_ARC_evict_miss_on_all_queues(cache_t *cache,
                                             const request_t *req);
static void _LP_ARC_replace(cache_t *cache, const request_t *req);
static cache_obj_t *_LP_ARC_to_evict_miss_on_all_queues(cache_t *cache,
                                                        const request_t *req);
static cache_obj_t *_LP_ARC_to_replace(cache_t *cache, const request_t *req);

static bool LP_ARC_get_debug(cache_t *cache, const request_t *req);

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
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *LP_ARC_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LP_ARC", ccache_params, cache_specific_params);
  cache->cache_init = LP_ARC_init;
  cache->cache_free = LP_ARC_free;
  cache->get = LP_ARC_get;
  cache->find = LP_ARC_find;
  cache->insert = LP_ARC_insert;
  cache->evict = LP_ARC_evict;
  cache->remove = LP_ARC_remove;
  cache->to_evict = LP_ARC_to_evict;
  cache->can_insert = cache_can_insert_default;
  cache->get_occupied_byte = LP_ARC_get_occupied_byte;
  cache->get_n_obj = LP_ARC_get_n_obj;

  if (ccache_params.consider_obj_metadata) {
    // two pointer + ghost metadata
    cache->obj_md_size = 8 * 2 + 8 * 3;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc_n(LP_ARC_params_t, 1);
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);
  params->p = 0;

  common_cache_params_t ccache_params_local = ccache_params;
  params->T1 = Clock_init(ccache_params_local, NULL);
  params->B1 = LRU_init(ccache_params_local, NULL);
  params->T2 = Clock_init(ccache_params_local, NULL);
  params->B2 = LRU_init(ccache_params_local, NULL);

  params->curr_obj_in_L1_ghost = false;
  params->curr_obj_in_L2_ghost = false;
  params->vtime_last_req_in_ghost = -1;
  params->req_local = new_request();

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "LP-ARC-Clock");

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void LP_ARC_free(cache_t *cache) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);
  params->T1->cache_free(params->T1);
  params->T2->cache_free(params->T2);
  params->B1->cache_free(params->B1);
  params->B2->cache_free(params->B2);

  free_request(params->req_local);
  my_free(sizeof(LP_ARC_params_t), params);
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
static bool LP_ARC_get(cache_t *cache, const request_t *req) {
  // LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);

  return LP_ARC_get_debug(cache, req);
  // return cache_get_base(cache, req);
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
static cache_obj_t *LP_ARC_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);

  cache_obj_t *obj_t1 = params->T1->find(params->T1, req, false);
  cache_obj_t *obj_t2 = params->T2->find(params->T2, req, false);
  DEBUG_ASSERT(obj_t1 == NULL || obj_t2 == NULL);
  cache_obj_t *obj = obj_t1 ? obj_t1 : obj_t2;

  if (!update_cache) {
    return obj;
  }

  cache_obj_t *obj_b1 = params->B1->find(params->B1, req, false);
  cache_obj_t *obj_b2 = params->B2->find(params->B2, req, false);
  DEBUG_ASSERT(obj_b1 == NULL || obj_b2 == NULL);
  cache_obj_t *obj_ghost = obj_b1 ? obj_b1 : obj_b2;
  DEBUG_ASSERT(obj == NULL || obj_ghost == NULL);

  if (obj == NULL && obj_ghost == NULL) {
    return NULL;
  }

  params->curr_obj_in_L1_ghost = false;
  params->curr_obj_in_L2_ghost = false;

  int64_t b1_size = params->B1->get_occupied_byte(params->B1);
  int64_t b2_size = params->B2->get_occupied_byte(params->B2);

  if (obj_ghost != NULL) {
    params->vtime_last_req_in_ghost = cache->n_req;
    // cache miss, but hit on thost
    if (obj_b1 != NULL) {
      params->curr_obj_in_L1_ghost = true;
      // case II: x in L1_ghost
      double delta = MAX((double)b2_size / b1_size, 1);
      params->p = MIN(params->p + delta, cache->cache_size);
      bool removed = params->B1->remove(params->B1, obj_b1->obj_id);
      DEBUG_ASSERT(removed);
    } else {
      params->curr_obj_in_L2_ghost = true;
      // case III: x in L2_ghost
      double delta = MAX((double)b1_size / b2_size, 1);
      params->p = MAX(params->p - delta, 0);
      bool removed = params->B2->remove(params->B2, obj_b2->obj_id);
      DEBUG_ASSERT(removed);
    }
  } else {
    // cache hit, case I: x in L1_data or L2_data
    if (obj_t1 != NULL) {
      params->T1->remove(params->T1, obj_t1->obj_id);
      params->T2->get(params->T2, req);
      DEBUG_ASSERT(params->B2->find(params->B2, req, false) == NULL);
    } else {
      // move to LRU2 head
      cache_obj_t *obj_tmp = params->T2->find(params->T2, req, true);
      assert(obj_tmp == obj_t2);
    }
  }

  return obj;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * eviction should be
 * performed before calling this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
static cache_obj_t *LP_ARC_insert(cache_t *cache, const request_t *req) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);

  cache_obj_t *obj = NULL;

  if (params->vtime_last_req_in_ghost == cache->n_req &&
      (params->curr_obj_in_L1_ghost || params->curr_obj_in_L2_ghost)) {
    // insert to L2 data head
    obj = params->T2->insert(params->T2, req);
    DEBUG_ASSERT(params->B2->find(params->B2, req, false) == NULL);

    params->curr_obj_in_L1_ghost = false;
    params->curr_obj_in_L2_ghost = false;
    params->vtime_last_req_in_ghost = -1;
  } else {
    // insert to L1 data head
    obj = params->T1->insert(params->T1, req);
  }

  return obj;
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
static cache_obj_t *LP_ARC_to_evict(cache_t *cache, const request_t *req) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);
  cache->to_evict_candidate_gen_vtime = cache->n_req;
  if (params->vtime_last_req_in_ghost == cache->n_req &&
      (params->curr_obj_in_L1_ghost || params->curr_obj_in_L2_ghost)) {
    cache->to_evict_candidate = _LP_ARC_to_replace(cache, req);
  } else {
    cache->to_evict_candidate = _LP_ARC_to_evict_miss_on_all_queues(cache, req);
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
 * @param evicted_obj if not NULL, return the evicted object to caller
 */
static void LP_ARC_evict(cache_t *cache, const request_t *req) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);
  if (params->vtime_last_req_in_ghost == cache->n_req &&
      (params->curr_obj_in_L1_ghost || params->curr_obj_in_L2_ghost)) {
    _LP_ARC_replace(cache, req);
  } else {
    _LP_ARC_evict_miss_on_all_queues(cache, req);
  }
  cache->to_evict_candidate_gen_vtime = -1;
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
static bool LP_ARC_remove(cache_t *cache, const obj_id_t obj_id) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);
  bool removed = false;
  removed |= params->T1->remove(params->T1, obj_id);
  removed |= params->T2->remove(params->T2, obj_id);

  return removed;
}

static int64_t LP_ARC_get_occupied_byte(const cache_t *cache) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);
  return params->T1->get_occupied_byte(params->T1) +
         params->T2->get_occupied_byte(params->T2);
}

static int64_t LP_ARC_get_n_obj(const cache_t *cache) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);
  return params->T1->get_n_obj(params->T1) + params->T2->get_n_obj(params->T2);
}

// ***********************************************************************
// ****                                                               ****
// ****                  cache internal functions                     ****
// ****                                                               ****
// ***********************************************************************
/* finding the eviction candidate in _LP_ARC_replace but do not perform eviction
 */
static cache_obj_t *_LP_ARC_to_replace(cache_t *cache, const request_t *req) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);

  cache_obj_t *obj = NULL;
  int64_t t1_size = params->T1->get_occupied_byte(params->T1);

  if (t1_size > 0 && (t1_size > params->p ||
                      (t1_size == params->p && params->curr_obj_in_L2_ghost))) {
    // delete the LRU in L1 data, move to L1_ghost
    obj = params->T1->to_evict(params->T1, req);
  } else {
    // delete the item in L2 data, move to L2_ghost
    obj = params->T2->to_evict(params->T2, req);
  }
  DEBUG_ASSERT(obj != NULL);
  return obj;
}

/* the REPLACE function in the paper */
static void _LP_ARC_replace(cache_t *cache, const request_t *req) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);

  int64_t t1_size = params->T1->get_occupied_byte(params->T1);
  int64_t t2_size = params->T2->get_occupied_byte(params->T2);

  bool cond1 = t1_size > 0;
  bool cond2 = t1_size > params->p;
  bool cond3 = t1_size == params->p && params->curr_obj_in_L2_ghost;
  bool cond4 = t2_size == 0;

  if ((cond1 && (cond2 || cond3)) || cond4) {
    // delete the LRU in L1 data, move to L1_ghost
    cache_obj_t *obj = params->T1->to_evict(params->T1, req);
    DEBUG_ASSERT(obj != NULL);
    copy_cache_obj_to_request(params->req_local, obj);
    bool in_g = params->B1->get(params->B1, params->req_local);
    DEBUG_ASSERT(in_g == false);
    params->T1->evict(params->T1, req);
  } else {
    // delete the item in L2 data, move to L2_ghost
    cache_obj_t *obj = params->T2->to_evict(params->T2, req);
    DEBUG_ASSERT(obj != NULL);
    copy_cache_obj_to_request(params->req_local, obj);
    bool in_g = params->B2->get(params->B2, params->req_local);
    DEBUG_ASSERT(in_g == false);
    params->T2->evict(params->T2, req);
  }
}

/* finding the eviction candidate in _LP_ARC_evict_miss_on_all_queues, but do
 * not perform eviction */
static cache_obj_t *_LP_ARC_to_evict_miss_on_all_queues(cache_t *cache,
                                                        const request_t *req) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);

  int64_t t1_size = params->T1->get_occupied_byte(params->T1);
  int64_t b1_size = params->B1->get_occupied_byte(params->B1);

  int64_t incoming_size = req->obj_size + cache->obj_md_size;
  if (t1_size + b1_size + incoming_size > cache->cache_size) {
    // case A: L1 = T1 U B1 has exactly c pages
    if (b1_size > 0) {
      return _LP_ARC_to_replace(cache, req);
    } else {
      // T1 >= c, L1 data size is too large, ghost is empty, so evict from L1
      // data
      return params->T1->to_evict(params->T1, req);
    }
  } else {
    return _LP_ARC_to_replace(cache, req);
  }
}

/* this is the case IV in the paper */
static void _LP_ARC_evict_miss_on_all_queues(cache_t *cache,
                                             const request_t *req) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);

  int64_t t1_size = params->T1->get_occupied_byte(params->T1);
  int64_t b1_size = params->B1->get_occupied_byte(params->B1);

  int64_t incoming_size = req->obj_size + cache->obj_md_size;
  if (t1_size + b1_size + incoming_size > cache->cache_size) {
    // case A: L1 = T1 U B1 has exactly c pages
    if (b1_size > 0) {
      // if T1 < c (ghost is not empty),
      // delete the LRU of the L1 ghost, and replace
      // we do not use t1_size < cache->cache_size
      // because it does not work for variable size objects
      params->B1->evict(params->B1, req);
      return _LP_ARC_replace(cache, req);
    } else {
      // T1 >= c, L1 data size is too large, ghost is empty, so evict from L1
      // data
      return params->T1->evict(params->T1, req);
    }
  } else {
    int64_t t2_size = params->T2->get_occupied_byte(params->T2);
    DEBUG_ASSERT(t1_size + b1_size < cache->cache_size);
    while (t1_size + b1_size + t2_size +
               params->B2->get_occupied_byte(params->B2) >=
           cache->cache_size * 2) {
      // delete the LRU end of the L2 ghost
      params->B2->evict(params->B2, req);
    }
    return _LP_ARC_replace(cache, req);
  }
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *LP_ARC_current_params(LP_ARC_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "\n");
  return params_str;
}

static void LP_ARC_parse_params(cache_t *cache,
                                const char *cache_specific_params) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  // char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *key = strsep((char **)&params_str, "=");
    // char *value = strsep((char **)&params_str, ",");

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", LP_ARC_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }

  free(old_params_str);
}

// ***********************************************************************
// ****                                                               ****
// ****                       debug functions                         ****
// ****                                                               ****
// ***********************************************************************
static void print_cache(cache_t *cache) {
  LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);
  printf("T1: ");
  params->T1->print_cache(params->T1);
  printf("T2: ");
  params->T2->print_cache(params->T2);
  printf("B1: ");
  params->B1->print_cache(params->B1);
  printf("B2: ");
  params->B2->print_cache(params->B2);
}

static bool LP_ARC_get_debug(cache_t *cache, const request_t *req) {
  // LP_ARC_params_t *params = (LP_ARC_params_t *)(cache->eviction_params);

  cache->n_req += 1;

  // printf("%ld obj_id %ld: p %.2lf\n", cache->n_req, req->obj_id, params->p);
  // print_cache(cache);
  // printf("==================================\n");

  cache_obj_t *obj = cache->find(cache, req, true);

  if (obj != NULL) {
    return true;
  }

  while (cache->get_occupied_byte(cache) + req->obj_size + cache->obj_md_size >
         cache->cache_size) {
    cache->evict(cache, req);
  }

  cache->insert(cache, req);

  return false;
}

#ifdef __cplusplus
}
#endif
