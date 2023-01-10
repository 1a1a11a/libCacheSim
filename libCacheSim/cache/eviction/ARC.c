//
//  ARC cache replacement algorithm
//  https://www.usenix.org/conference/fast-03/arc-self-tuning-low-overhead-replacement-cache
//
//
//  cross checked with https://github.com/trauzti/cache/blob/master/ARC.py
//  one thing not clear in the paper is whether delta and p is int or float,
//  we used int as first,
//  but the implemnetation above used float, so we have changed to use float
//
//
//  libCacheSim
//
//  Created by Juncheng on 09/28/20.
//  Copyright © 2020 Juncheng. All rights reserved.
//

#include <string.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/ARC.h"
#include "../../include/libCacheSim/evictionAlgo/LRU.h"

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
  request_t *req_local;
} ARC_params_t;

static void ARC_parse_params(cache_t *cache, const char *cache_specific_params);
static void _ARC_replace(cache_t *cache, const request_t *req,
                         cache_obj_t *evicted_obj);

static void _ARC_replace(cache_t *cache, const request_t *req,
                         cache_obj_t *evicted_obj);

static void _ARC_print_cache_content(cache_t *cache);
static void _ARC_sanity_check(cache_t *cache, const request_t *req);
static inline void _ARC_sanity_check_full(cache_t *cache, const request_t *req,
                                          bool last);
bool ARC_get_debug(cache_t *cache, const request_t *req);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief initialize a ARC cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params ARC specific parameters, should be NULL
 */
cache_t *ARC_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("ARC", ccache_params);
  cache->cache_init = ARC_init;
  cache->cache_free = ARC_free;
  cache->get = ARC_get;
  cache->check = ARC_check;
  cache->insert = ARC_insert;
  cache->evict = ARC_evict;
  cache->remove = ARC_remove;
  cache->to_evict = ARC_to_evict;
  cache->init_params = cache_specific_params;

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
void ARC_free(cache_t *cache) {
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
bool ARC_get(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

#ifdef DEBUG_MODE
  return ARC_get_debug(cache, req);
#else
  return cache_get_base(cache, req);
#endif
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
bool ARC_check(cache_t *cache, const request_t *req, const bool update_cache) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  params->curr_obj_in_L1_ghost = false;
  params->curr_obj_in_L2_ghost = false;

  // cache->last_request_metadata = (void *)"check1";
  // _ARC_sanity_check_full(cache, req, false);

  bool cache_hit = false;
  int lru_id = -1;
  cache_obj_t *obj = cache_get_obj(cache, req);
  if (obj != NULL) {
    lru_id = obj->ARC.lru_id;
    if (obj->ARC.ghost) {
      // ghost hit
      if (obj->ARC.lru_id == 1) {
        params->curr_obj_in_L1_ghost = true;
      } else {
        params->curr_obj_in_L2_ghost = true;
      }
    } else {
      // data hit
      cache_hit = true;
    }
  } else {
    // cache miss
    return false;
  }

  if (!update_cache) return cache_hit;

#ifdef USE_BELADY
  obj->next_access_vtime = req->next_access_vtime;
#endif

  if (!cache_hit) {
    // cache miss, but hit on thost
    if (params->curr_obj_in_L1_ghost) {
      // case II: x in L1_ghost
      DEBUG_ASSERT(params->L1_ghost_size >= 1);
      double delta =
          MAX((double)params->L2_ghost_size / params->L1_ghost_size, 1);
      params->p = MIN(params->p + delta, cache->cache_size);
      _ARC_replace(cache, req, NULL);
      params->L1_ghost_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
    }
    if (params->curr_obj_in_L2_ghost) {
      // case III: x in L2_ghost
      DEBUG_ASSERT(params->L2_ghost_size >= 1);
      double delta =
          MAX((double)params->L1_ghost_size / params->L2_ghost_size, 1);
      params->p = MAX(params->p - delta, 0);
      _ARC_replace(cache, req, NULL);
      params->L2_ghost_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L2_ghost_head, &params->L2_ghost_tail, obj);
    }
    hashtable_delete(cache->hashtable, obj);
  } else {
    // cache hit, case I: x in L1_data or L2_data
#ifdef USE_BELADY
    if (obj->next_access_vtime == INT64_MAX) {
      return cache_hit;
    }
#endif

    if (lru_id == 1) {
      // move to LRU2
      obj->ARC.lru_id = 2;
      remove_obj_from_list(&params->L1_data_head, &params->L1_data_tail, obj);
      prepend_obj_to_head(&params->L2_data_head, &params->L2_data_tail, obj);
      params->L1_data_size -= obj->obj_size + cache->obj_md_size;
      params->L2_data_size += req->obj_size + cache->obj_md_size;
    } else {
      // move to LRU2 head
      move_obj_to_head(&params->L2_data_head, &params->L2_data_tail, obj);
    }
  }

  return cache_hit;
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
cache_obj_t *ARC_insert(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  cache_obj_t *obj = hashtable_insert(cache->hashtable, req);
#ifdef USE_BELADY
  obj->next_access_vtime = req->next_access_vtime;
#endif
  if (params->curr_obj_in_L1_ghost || params->curr_obj_in_L2_ghost) {
    // insert to L2 data head
    obj->ARC.lru_id = 2;
    prepend_obj_to_head(&params->L2_data_head, &params->L2_data_tail, obj);
    params->L2_data_size += req->obj_size + cache->obj_md_size;
  } else {
    // insert to L1 data head
    obj->ARC.lru_id = 1;
    prepend_obj_to_head(&params->L1_data_head, &params->L1_data_tail, obj);
    params->L1_data_size += req->obj_size + cache->obj_md_size;
  }

  params->curr_obj_in_L1_ghost = false;
  params->curr_obj_in_L2_ghost = false;

  cache->occupied_byte += req->obj_size + cache->obj_md_size;
  cache->n_obj += 1;

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
cache_obj_t *ARC_to_evict(cache_t *cache) {
  // does not support to_evict
  DEBUG_ASSERT(false);
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
void ARC_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  // do not support as of now
  DEBUG_ASSERT(evicted_obj == NULL);

  int64_t incoming_size = +req->obj_size + cache->obj_md_size;
  if (params->L1_data_size + params->L1_ghost_size + incoming_size >
      cache->cache_size) {
    // case A: L1 = T1 U B1 has exactly c pages
    if (params->L1_ghost_size > 0) {
      // if T1 < c (ghost is not empty),
      // delete the LRU of the L1 ghost, and replace
      // we do not use params->L1_data_size < cache->cache_size
      // because it does not work for variable size objects
      cache_obj_t *obj = params->L1_ghost_tail;
      DEBUG_ASSERT(obj != NULL);
      DEBUG_ASSERT(obj->ARC.ghost);
      int64_t sz = obj->obj_size + cache->obj_md_size;
      params->L1_ghost_size -= sz;
      remove_obj_from_list(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
      hashtable_delete(cache->hashtable, obj);

      return _ARC_replace(cache, req, evicted_obj);
    } else {
      // T1 >= c, L1 data size is too large, ghost is empty, so evict from L1
      // data
      cache_obj_t *obj = params->L1_data_tail;
      int64_t sz = obj->obj_size + cache->obj_md_size;
      params->L1_data_size -= sz;
      cache->occupied_byte -= sz;
      cache->n_obj -= 1;
      remove_obj_from_list(&params->L1_data_head, &params->L1_data_tail, obj);
      DEBUG_ASSERT(params->L1_data_tail != NULL);
      hashtable_delete(cache->hashtable, obj);
    }
  } else {
    DEBUG_ASSERT(params->L1_data_size + params->L1_ghost_size <
                 cache->cache_size);
    if (params->L1_data_size + params->L1_ghost_size + params->L2_data_size +
            params->L2_ghost_size >=
        cache->cache_size * 2) {
      // delete the LRU end of the L2 ghost
      cache_obj_t *obj = params->L2_ghost_tail;
      params->L2_ghost_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L2_ghost_head, &params->L2_ghost_tail, obj);
      hashtable_delete(cache->hashtable, obj);
    }
    return _ARC_replace(cache, req, evicted_obj);
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
bool ARC_remove(cache_t *cache, const obj_id_t obj_id) {
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
/* the REPLACE function in the paper */
static void _ARC_replace(cache_t *cache, const request_t *req,
                         cache_obj_t *evicted_obj) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  cache_obj_t *obj = NULL;

  if (params->L1_data_size > 0 &&
      (params->L1_data_size > params->p ||
       (params->L1_data_size == params->p && params->curr_obj_in_L2_ghost))) {
    // delete the LRU in L1 data, move to L1_ghost
    obj = params->L1_data_tail;
    DEBUG_ASSERT(obj != NULL);
    params->L1_data_size -= obj->obj_size + cache->obj_md_size;
    params->L1_ghost_size += obj->obj_size + cache->obj_md_size;
    remove_obj_from_list(&params->L1_data_head, &params->L1_data_tail, obj);
    prepend_obj_to_head(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
  } else {
    // delete the item in L2 data, move to L2_ghost
    obj = params->L2_data_tail;
    DEBUG_ASSERT(obj != NULL);
    params->L2_data_size -= obj->obj_size + cache->obj_md_size;
    params->L2_ghost_size += obj->obj_size + cache->obj_md_size;
    remove_obj_from_list(&params->L2_data_head, &params->L2_data_tail, obj);
    prepend_obj_to_head(&params->L2_ghost_head, &params->L2_ghost_tail, obj);
  }
  obj->ARC.ghost = true;
  cache->occupied_byte -= obj->obj_size + cache->obj_md_size;
  cache->n_obj -= 1;
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
  char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

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
static void _ARC_print_cache_content(cache_t *cache) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  printf("p: %lf\n", params->p);
  cache_obj_t *obj = params->L1_data_head;
  printf("L1_data: ");
  while (obj != NULL) {
    printf("%ld ", obj->obj_id);
    obj = obj->queue.next;
  }
  printf("\n");

  obj = params->L1_ghost_head;
  printf("L1_ghost: ");
  while (obj != NULL) {
    printf("%ld ", obj->obj_id);
    obj = obj->queue.next;
  }
  printf("\n");

  obj = params->L2_data_head;
  printf("L2_data: ");
  while (obj != NULL) {
    printf("%ld ", obj->obj_id);
    obj = obj->queue.next;
  }
  printf("\n");

  obj = params->L2_ghost_head;
  printf("L2_ghost: ");
  while (obj != NULL) {
    printf("%ld ", obj->obj_id);
    obj = obj->queue.next;
  }
  printf("\n");
}

static void _ARC_sanity_check(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  printf("%ld %ld %ld, %s %ld + %ld + %ld + %ld = %ld\n", cache->n_req,
         cache->n_obj, req->obj_id, ((char *)(cache->last_request_metadata)),
         params->L1_data_size, params->L2_data_size, params->L1_ghost_size,
         params->L2_ghost_size,
         params->L1_data_size + params->L2_data_size + params->L1_ghost_size +
             params->L2_ghost_size);

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

static inline void _ARC_sanity_check_full(cache_t *cache, const request_t *req,
                                          bool last) {
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

  if (last) {
    printf("***************************************\n");
  }
}

bool ARC_get_debug(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  cache->n_req += 1;
  cache->last_request_metadata = (void *)"None";

  _ARC_sanity_check_full(cache, req, false);

  bool cache_hit = cache->check(cache, req, true);

  cache->last_request_metadata = cache_hit ? (void *)"hit" : (void *)"miss";

  _ARC_sanity_check_full(cache, req, false);

  if (cache_hit) {
    // _ARC_print_cache_content(cache);
    return cache_hit;
  }

  while (cache->occupied_byte + req->obj_size + cache->obj_md_size >
         cache->cache_size) {
    cache->evict(cache, req, NULL);
  }

  _ARC_sanity_check_full(cache, req, false);

  cache->insert(cache, req);

  _ARC_sanity_check_full(cache, req, true);

  // _ARC_print_cache_content(cache);
  return cache_hit;
}

#ifdef __cplusplus
}
#endif
