//
//  ARC cache replacement algorithm
//  https://www.usenix.org/conference/fast-03/arc-self-tuning-low-overhead-replacement-cache
//
//
//  cross checked with https://github.com/trauzti/cache/blob/master/ARC.py
//  one thing not clear in the paper is whether delta and p is int or float,
//  we used int as first,
//  but the implementation above used float, so we have changed to use float
//
//
//  libCacheSim
//
//  Created by Juncheng on 09/28/20.
//  Copyright © 2020 Juncheng. All rights reserved.
//

#include <string.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define DEBUG_MODE
// #undef DEBUG_MODE
// #define USE_BELADY

typedef struct ARC_params {
  // L1_data is T1 in the paper, L1_ghost is B1 in the paper
  int64_t L1_data_size;
  int64_t L2_data_size;
  int64_t L1_ghost_size;
  int64_t L2_ghost_size;

  cache_obj_t *L1_data_head;
  cache_obj_t *L1_data_tail;
  cache_obj_t *L1_ghost_head;
  cache_obj_t *L1_ghost_tail;

  cache_obj_t *L2_data_head;
  cache_obj_t *L2_data_tail;
  cache_obj_t *L2_ghost_head;
  cache_obj_t *L2_ghost_tail;

  double p;
  bool curr_obj_in_L1_ghost;
  bool curr_obj_in_L2_ghost;
  int64_t vtime_last_req_in_ghost;
  request_t *req_local;
} ARC_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void ARC_parse_params(cache_t *cache, const char *cache_specific_params);
static void ARC_free(cache_t *cache);
static bool ARC_get(cache_t *cache, const request_t *req);
static cache_obj_t *ARC_find(cache_t *cache, const request_t *req,
                             const bool update_cache);
static cache_obj_t *ARC_insert(cache_t *cache, const request_t *req);
static cache_obj_t *ARC_to_evict(cache_t *cache, const request_t *req);
static void ARC_evict(cache_t *cache, const request_t *req);
static bool ARC_remove(cache_t *cache, const obj_id_t obj_id);

/* internal functions */
/* this is the case IV in the paper */
static void _ARC_evict_miss_on_all_queues(cache_t *cache, const request_t *req);
static void _ARC_replace(cache_t *cache, const request_t *req);
static cache_obj_t *_ARC_to_evict_miss_on_all_queues(cache_t *cache,
                                                     const request_t *req);
static cache_obj_t *_ARC_to_replace(cache_t *cache, const request_t *req);

/* debug functions */
static void print_cache(cache_t *cache);
static void _ARC_sanity_check(cache_t *cache, const request_t *req);
static inline void _ARC_sanity_check_full(cache_t *cache, const request_t *req);
static bool ARC_get_debug(cache_t *cache, const request_t *req);

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
cache_t *ARC_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("ARC", ccache_params, cache_specific_params);
  cache->cache_init = ARC_init;
  cache->cache_free = ARC_free;
  cache->get = ARC_get;
  cache->find = ARC_find;
  cache->insert = ARC_insert;
  cache->evict = ARC_evict;
  cache->remove = ARC_remove;
  cache->to_evict = ARC_to_evict;
  cache->can_insert = cache_can_insert_default;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->get_n_obj = cache_get_n_obj_default;

  if (ccache_params.consider_obj_metadata) {
    // two pointer + ghost metadata
    cache->obj_md_size = 8 * 2 + 8 * 3;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc_n(ARC_params_t, 1);
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  params->p = 0;

  params->L1_data_size = 0;
  params->L2_data_size = 0;
  params->L1_ghost_size = 0;
  params->L2_ghost_size = 0;
  params->L1_data_head = NULL;
  params->L1_data_tail = NULL;
  params->L1_ghost_head = NULL;
  params->L1_ghost_tail = NULL;
  params->L2_data_head = NULL;
  params->L2_data_tail = NULL;
  params->L2_ghost_head = NULL;
  params->L2_ghost_tail = NULL;

  params->curr_obj_in_L1_ghost = false;
  params->curr_obj_in_L2_ghost = false;
  params->vtime_last_req_in_ghost = -1;
  params->req_local = new_request();

#ifdef USE_BELADY
  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "ARC_Belady");
#endif

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void ARC_free(cache_t *cache) {
  ARC_params_t *ARC_params = (ARC_params_t *)(cache->eviction_params);
  free_request(ARC_params->req_local);
  my_free(sizeof(ARC_params_t), ARC_params);
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
static bool ARC_get(cache_t *cache, const request_t *req) {
#ifdef DEBUG_MODE
  return ARC_get_debug(cache, req);
#else

#if defined(TRACK_DEMOTION)
  if (cache->n_req % 100000 == 0) {
    printf(
        "l1 data size: %lu, %.4lf, l1 ghost size: %lu, l2 data size: %lu, l2 "
        "ghost size: %lu\n",
        params->L1_data_size,
        params->L1_data_size /
            (double)(params->L1_data_size + params->L2_data_size),
        params->L1_ghost_size, params->L2_data_size, params->L2_ghost_size);
  }
#endif

  return cache_get_base(cache, req);
#endif
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
static cache_obj_t *ARC_find(cache_t *cache, const request_t *req,
                             const bool update_cache) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  cache_obj_t *obj = cache_find_base(cache, req, update_cache);

  if (!update_cache) {
    return obj->ARC.ghost ? NULL : obj;
  }

  if (obj == NULL) {
    return NULL;
  }

  params->curr_obj_in_L1_ghost = false;
  params->curr_obj_in_L2_ghost = false;

  int lru_id = obj->ARC.lru_id;
  cache_obj_t *ret = obj;

  if (obj->ARC.ghost) {
    // ghost hit
    ret = NULL;
    params->vtime_last_req_in_ghost = cache->n_req;
    // cache miss, but hit on thost
    if (obj->ARC.lru_id == 1) {
      params->curr_obj_in_L1_ghost = true;
      // case II: x in L1_ghost
      DEBUG_ASSERT(params->L1_ghost_size >= 1);
      double delta =
          MAX((double)params->L2_ghost_size / params->L1_ghost_size, 1);
      params->p = MIN(params->p + delta, cache->cache_size);
      params->L1_ghost_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
    } else {
      params->curr_obj_in_L2_ghost = true;
      // case III: x in L2_ghost
      DEBUG_ASSERT(params->L2_ghost_size >= 1);
      double delta =
          MAX((double)params->L1_ghost_size / params->L2_ghost_size, 1);
      params->p = MAX(params->p - delta, 0);
      params->L2_ghost_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L2_ghost_head, &params->L2_ghost_tail, obj);
    }

    hashtable_delete(cache->hashtable, obj);
  } else {
    // cache hit, case I: x in L1_data or L2_data
#ifdef USE_BELADY
    if (obj->next_access_vtime == INT64_MAX) {
      return ret;
    }
#endif

    if (lru_id == 1) {
      // move to LRU2
      obj->ARC.lru_id = 2;
      remove_obj_from_list(&params->L1_data_head, &params->L1_data_tail, obj);
      prepend_obj_to_head(&params->L2_data_head, &params->L2_data_tail, obj);

#if defined(TRACK_DEMOTION)
      obj->misc.next_access_vtime = req->next_access_vtime;
      printf("%ld keep %ld %ld\n", cache->n_req, obj->create_time,
             obj->misc.next_access_vtime);
#endif

      params->L1_data_size -= obj->obj_size + cache->obj_md_size;
      params->L2_data_size += obj->obj_size + cache->obj_md_size;
    } else {
      // move to LRU2 head
      move_obj_to_head(&params->L2_data_head, &params->L2_data_tail, obj);
    }
  }

  return ret;
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
static cache_obj_t *ARC_insert(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  cache_obj_t *obj = cache_insert_base(cache, req);

  if (params->vtime_last_req_in_ghost == cache->n_req &&
      (params->curr_obj_in_L1_ghost || params->curr_obj_in_L2_ghost)) {
    // insert to L2 data head
    obj->ARC.lru_id = 2;
    prepend_obj_to_head(&params->L2_data_head, &params->L2_data_tail, obj);
    params->L2_data_size += req->obj_size + cache->obj_md_size;

    params->curr_obj_in_L1_ghost = false;
    params->curr_obj_in_L2_ghost = false;
    params->vtime_last_req_in_ghost = -1;
  } else {
    // insert to L1 data head
    obj->ARC.lru_id = 1;
    prepend_obj_to_head(&params->L1_data_head, &params->L1_data_tail, obj);
    params->L1_data_size += req->obj_size + cache->obj_md_size;
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
static cache_obj_t *ARC_to_evict(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache->to_evict_candidate_gen_vtime = cache->n_req;
  if (params->vtime_last_req_in_ghost == cache->n_req &&
      (params->curr_obj_in_L1_ghost || params->curr_obj_in_L2_ghost)) {
    cache->to_evict_candidate = _ARC_to_replace(cache, req);
  } else {
    cache->to_evict_candidate = _ARC_to_evict_miss_on_all_queues(cache, req);
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
static void ARC_evict(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  if (params->vtime_last_req_in_ghost == cache->n_req &&
      (params->curr_obj_in_L1_ghost || params->curr_obj_in_L2_ghost)) {
    _ARC_replace(cache, req);
  } else {
    _ARC_evict_miss_on_all_queues(cache, req);
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
static bool ARC_remove(cache_t *cache, const obj_id_t obj_id) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);

  if (obj == NULL) {
    return false;
  }

  if (obj->ARC.ghost) {
    if (obj->ARC.lru_id == 1) {
      params->L1_ghost_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
    } else {
      params->L2_ghost_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L2_ghost_head, &params->L2_ghost_tail, obj);
    }
  } else {
    if (obj->ARC.lru_id == 1) {
      params->L1_data_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L1_data_head, &params->L1_data_tail, obj);
    } else {
      params->L2_data_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L2_data_head, &params->L2_data_tail, obj);
    }
    cache_remove_obj_base(cache, obj, true);
  }

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                  cache internal functions                     ****
// ****                                                               ****
// ***********************************************************************
/* finding the eviction candidate in _ARC_replace but do not perform eviction */
static cache_obj_t *_ARC_to_replace(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  cache_obj_t *obj = NULL;

  bool cond1 = params->L1_data_size > 0;
  bool cond2 = params->L1_data_size > params->p;
  bool cond3 =
      params->L1_data_size == params->p && params->curr_obj_in_L2_ghost;
  bool cond4 = params->L2_data_size == 0;

  if ((cond1 && (cond2 || cond3)) || cond4) {
    // delete the LRU in L1 data, move to L1_ghost
    obj = params->L1_data_tail;
  } else {
    // delete the item in L2 data, move to L2_ghost
    obj = params->L2_data_tail;
  }

  DEBUG_ASSERT(obj != NULL);
  return obj;
}

static void _ARC_evict_L1_data(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_obj_t *obj = params->L1_data_tail;
  DEBUG_ASSERT(obj != NULL);

#if defined(TRACK_DEMOTION)
  printf("%ld demote %ld %ld\n", cache->n_req, obj->create_time,
         obj->misc.next_access_vtime);
#endif

  cache_evict_base(cache, obj, false);

  params->L1_data_size -= obj->obj_size + cache->obj_md_size;
  params->L1_ghost_size += obj->obj_size + cache->obj_md_size;
  remove_obj_from_list(&params->L1_data_head, &params->L1_data_tail, obj);
  prepend_obj_to_head(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
  obj->ARC.ghost = true;
}

static void _ARC_evict_L1_data_no_ghost(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_obj_t *obj = params->L1_data_tail;
  DEBUG_ASSERT(obj != NULL);

#if defined(TRACK_DEMOTION)
  printf("%ld demote %ld %ld\n", cache->n_req, obj->create_time,
         obj->misc.next_access_vtime);
#endif

  remove_obj_from_list(&params->L1_data_head, &params->L1_data_tail, obj);
  params->L1_data_size -= obj->obj_size + cache->obj_md_size;

  cache_evict_base(cache, obj, true);
}

static void _ARC_evict_L2_data(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_obj_t *obj = params->L2_data_tail;
  DEBUG_ASSERT(obj != NULL);

  params->L2_data_size -= obj->obj_size + cache->obj_md_size;
  params->L2_ghost_size += obj->obj_size + cache->obj_md_size;
  remove_obj_from_list(&params->L2_data_head, &params->L2_data_tail, obj);
  prepend_obj_to_head(&params->L2_ghost_head, &params->L2_ghost_tail, obj);

  obj->ARC.ghost = true;

  cache_evict_base(cache, obj, false);
}

static void _ARC_evict_L1_ghost(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_obj_t *obj = params->L1_ghost_tail;
  DEBUG_ASSERT(obj != NULL);
  DEBUG_ASSERT(obj->ARC.ghost);
  int64_t sz = obj->obj_size + cache->obj_md_size;
  params->L1_ghost_size -= sz;
  remove_obj_from_list(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
  hashtable_delete(cache->hashtable, obj);
}

static void _ARC_evict_L2_ghost(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_obj_t *obj = params->L2_ghost_tail;
  DEBUG_ASSERT(obj != NULL);
  DEBUG_ASSERT(obj->ARC.ghost);
  int64_t sz = obj->obj_size + cache->obj_md_size;
  params->L2_ghost_size -= sz;
  remove_obj_from_list(&params->L2_ghost_head, &params->L2_ghost_tail, obj);
  hashtable_delete(cache->hashtable, obj);
}

/* the REPLACE function in the paper */
static void _ARC_replace(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);


  bool cond1 = params->L1_data_size > 0;
  bool cond2 = params->L1_data_size > params->p;
  bool cond3 =
      params->L1_data_size == params->p && params->curr_obj_in_L2_ghost;
  bool cond4 = params->L2_data_size == 0;

  if ((cond1 && (cond2 || cond3)) || cond4) {
    // delete the LRU in L1 data, move to L1_ghost
    _ARC_evict_L1_data(cache, req);
  } else {
    // delete the item in L2 data, move to L2_ghost
    _ARC_evict_L2_data(cache, req);
  }
}

/* finding the eviction candidate in _ARC_evict_miss_on_all_queues, but do not
 * perform eviction */
static cache_obj_t *_ARC_to_evict_miss_on_all_queues(cache_t *cache,
                                                     const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  int64_t incoming_size = +req->obj_size + cache->obj_md_size;
  if (params->L1_data_size + params->L1_ghost_size + incoming_size >
      cache->cache_size) {
    // case A: L1 = T1 U B1 has exactly c pages
    if (params->L1_ghost_size > 0) {
      return _ARC_to_replace(cache, req);
    } else {
      // T1 >= c, L1 data size is too large, ghost is empty, so evict from L1
      // data
      return params->L1_data_tail;
    }
  } else {
    return _ARC_to_replace(cache, req);
  }
}

/* this is the case IV in the paper */
static void _ARC_evict_miss_on_all_queues(cache_t *cache,
                                          const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  int64_t incoming_size = req->obj_size + cache->obj_md_size;
  if (params->L1_data_size + params->L1_ghost_size + incoming_size >
      cache->cache_size) {
    // case A: L1 = T1 U B1 has exactly c pages
    if (params->L1_ghost_size > 0) {
      // if T1 < c (ghost is not empty),
      // delete the LRU of the L1 ghost, and replace
      // we do not use params->L1_data_size < cache->cache_size
      // because it does not work for variable size objects
      _ARC_evict_L1_ghost(cache, req);
      return _ARC_replace(cache, req);
    } else {
      // T1 >= c, L1 data size is too large, ghost is empty, so evict from L1
      // data
      return _ARC_evict_L1_data_no_ghost(cache, req);
    }
  } else {
    DEBUG_ASSERT(params->L1_data_size + params->L1_ghost_size <
                 cache->cache_size);
    if (params->L1_data_size + params->L1_ghost_size + params->L2_data_size +
            params->L2_ghost_size >=
        cache->cache_size * 2) {
      // delete the LRU end of the L2 ghost
      if (params->L2_ghost_size > 0) {
        // it maybe empty if object size is variable
        _ARC_evict_L2_ghost(cache, req);
      }
    }
    return _ARC_replace(cache, req);
  }
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *ARC_current_params(ARC_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "\n");
  return params_str;
}

static void ARC_parse_params(cache_t *cache,
                             const char *cache_specific_params) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;

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
      printf("parameters: %s\n", ARC_current_params(params));
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
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  cache_obj_t *obj = params->L1_data_head;
  printf("T1: ");
  while (obj != NULL) {
    printf("%ld ", (long)obj->obj_id);
    obj = obj->queue.next;
  }
  printf("\n");

  obj = params->L1_ghost_head;
  printf("B1: ");
  while (obj != NULL) {
    printf("%ld ", (long)obj->obj_id);
    obj = obj->queue.next;
  }
  printf("\n");

  obj = params->L2_data_head;
  printf("T2: ");
  while (obj != NULL) {
    printf("%ld ", (long)obj->obj_id);
    obj = obj->queue.next;
  }
  printf("\n");

  obj = params->L2_ghost_head;
  printf("B2: ");
  while (obj != NULL) {
    printf("%ld ", (long)obj->obj_id);
    obj = obj->queue.next;
  }
  printf("\n");
}

static void _ARC_sanity_check(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  DEBUG_ASSERT(params->L1_data_size >= 0);
  DEBUG_ASSERT(params->L1_ghost_size >= 0);
  DEBUG_ASSERT(params->L2_data_size >= 0);
  DEBUG_ASSERT(params->L2_ghost_size >= 0);

  if (params->L1_data_size > 0) {
    DEBUG_ASSERT(params->L1_data_head != NULL);
    DEBUG_ASSERT(params->L1_data_tail != NULL);
  }
  if (params->L1_ghost_size > 0) {
    DEBUG_ASSERT(params->L1_ghost_head != NULL);
    DEBUG_ASSERT(params->L1_ghost_tail != NULL);
  }
  if (params->L2_data_size > 0) {
    DEBUG_ASSERT(params->L2_data_head != NULL);
    DEBUG_ASSERT(params->L2_data_tail != NULL);
  }
  if (params->L2_ghost_size > 0) {
    DEBUG_ASSERT(params->L2_ghost_head != NULL);
    DEBUG_ASSERT(params->L2_ghost_tail != NULL);
  }

  DEBUG_ASSERT(params->L1_data_size + params->L2_data_size ==
               cache->occupied_byte);
  // DEBUG_ASSERT(params->L1_data_size + params->L2_data_size +
  //                  params->L1_ghost_size + params->L2_ghost_size <=
  //              cache->cache_size * 2);
  DEBUG_ASSERT(cache->occupied_byte <= cache->cache_size);
}

static inline void _ARC_sanity_check_full(cache_t *cache,
                                          const request_t *req) {
  // if (cache->n_req < 13200000) return;

  _ARC_sanity_check(cache, req);

  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  int64_t L1_data_byte = 0, L2_data_byte = 0;
  int64_t L1_ghost_byte = 0, L2_ghost_byte = 0;

  cache_obj_t *obj = params->L1_data_head;
  cache_obj_t *last_obj = NULL;
  while (obj != NULL) {
    DEBUG_ASSERT(obj->ARC.lru_id == 1);
    DEBUG_ASSERT(!obj->ARC.ghost);
    L1_data_byte += obj->obj_size;
    last_obj = obj;
    obj = obj->queue.next;
  }
  DEBUG_ASSERT(L1_data_byte == params->L1_data_size);
  DEBUG_ASSERT(last_obj == params->L1_data_tail);

  obj = params->L1_ghost_head;
  last_obj = NULL;
  while (obj != NULL) {
    DEBUG_ASSERT(obj->ARC.lru_id == 1);
    DEBUG_ASSERT(obj->ARC.ghost);
    L1_ghost_byte += obj->obj_size;
    last_obj = obj;
    obj = obj->queue.next;
  }
  DEBUG_ASSERT(L1_ghost_byte == params->L1_ghost_size);
  DEBUG_ASSERT(last_obj == params->L1_ghost_tail);

  obj = params->L2_data_head;
  last_obj = NULL;
  while (obj != NULL) {
    DEBUG_ASSERT(obj->ARC.lru_id == 2);
    DEBUG_ASSERT(!obj->ARC.ghost);
    L2_data_byte += obj->obj_size;
    last_obj = obj;
    obj = obj->queue.next;
  }
  DEBUG_ASSERT(L2_data_byte == params->L2_data_size);
  DEBUG_ASSERT(last_obj == params->L2_data_tail);

  obj = params->L2_ghost_head;
  last_obj = NULL;
  while (obj != NULL) {
    DEBUG_ASSERT(obj->ARC.lru_id == 2);
    DEBUG_ASSERT(obj->ARC.ghost);
    L2_ghost_byte += obj->obj_size;
    last_obj = obj;
    obj = obj->queue.next;
  }
  DEBUG_ASSERT(L2_ghost_byte == params->L2_ghost_size);
  DEBUG_ASSERT(last_obj == params->L2_ghost_tail);
}

static bool ARC_get_debug(cache_t *cache, const request_t *req) {

  cache->n_req += 1;

  _ARC_sanity_check_full(cache, req);
  // printf("%ld obj_id %ld, p %.2lf\n", cache->n_req, req->obj_id, params->p);
  // print_cache(cache);
  // printf("***************************************\n");

  cache_obj_t *obj = cache->find(cache, req, true);
  cache->last_request_metadata = obj != NULL ? (void *)"hit" : (void *)"miss";

  if (obj != NULL) {
    _ARC_sanity_check_full(cache, req);
    return true;
  }

  if (!cache->can_insert(cache, req)) {
    return false;
  }

  while (cache->occupied_byte + req->obj_size + cache->obj_md_size >
         cache->cache_size) {
    cache->evict(cache, req);
  }

  _ARC_sanity_check_full(cache, req);

  cache->insert(cache, req);
  _ARC_sanity_check_full(cache, req);

  return false;
}

#ifdef __cplusplus
}
#endif
