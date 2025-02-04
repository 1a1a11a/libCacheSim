//
// LIRS cache eviction policy implemented by multiple LRUs
//
// LIRS.c
// libcachesim
//
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define DEBUG_MODE 1

typedef struct LIRS_params {
  cache_t *LRU_s;
  cache_t *LRU_q;
  cache_t *LRU_nh;
  double hirs_ratio;
  uint64_t hirs_limit;
  uint64_t lirs_limit;
  uint64_t hirs_count;
  uint64_t lirs_count;
  uint64_t nonresident;
} LIRS_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void LIRS_free(cache_t *cache);
static bool LIRS_get(cache_t *cache, const request_t *req);
static cache_obj_t *LIRS_find(cache_t *cache, const request_t *req,
                              const bool update_cache);
static cache_obj_t *LIRS_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LIRS_to_evict(cache_t *cache, const request_t *req);
static void LIRS_evict(cache_t *cache, const request_t *req);
static bool LIRS_remove(cache_t *cache, const obj_id_t obj_id);

/* internal functions */
bool LIRS_can_insert(cache_t *cache, const request_t *req);
static void LIRS_prune(cache_t *cache);
static cache_obj_t *hit_RD_HIRinS(cache_t *cache, cache_obj_t *cache_obj_s,
                                  cache_obj_t *cache_obj_q);
static cache_obj_t *hit_NR_HIRinS(cache_t *cache, cache_obj_t *cache_obj_s);
static cache_obj_t *hit_RD_HIRinQ(cache_t *cache, cache_obj_t *cache_obj_q);
static void evictLIR(cache_t *cache);
static bool evictHIR(cache_t *cache);
static void limitStack(cache_t *cache);

/* debug functions*/
static void LIRS_print_cache(cache_t *cache);
static void LIRS_print_cache_compared_to_cacheus(cache_t *cache);

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
cache_t *LIRS_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("LIRS", ccache_params, cache_specific_params);
  cache->cache_init = LIRS_init;
  cache->cache_free = LIRS_free;
  cache->get = LIRS_get;
  cache->find = LIRS_find;
  cache->can_insert = LIRS_can_insert;
  cache->insert = LIRS_insert;
  cache->evict = LIRS_evict;
  cache->remove = LIRS_remove;
  cache->to_evict = LIRS_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = (LIRS_params_t *)malloc(sizeof(LIRS_params_t));
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);

  params->hirs_ratio = 0.01;
  params->hirs_limit =
      MAX(1, (uint64_t)(params->hirs_ratio * cache->cache_size));
  params->lirs_limit = cache->cache_size - params->hirs_limit;
  params->hirs_count = 0;
  params->lirs_count = 0;
  params->nonresident = 0;

  // initialize LRU for stack S and stack Q
  common_cache_params_t ccache_params_s = ccache_params;
  ccache_params_s.cache_size = params->lirs_limit;
  common_cache_params_t ccache_params_q = ccache_params;
  ccache_params_q.cache_size = params->hirs_limit;
  common_cache_params_t ccache_params_nh = ccache_params;

  params->LRU_s = LRU_init(ccache_params_s, NULL);
  params->LRU_q = LRU_init(ccache_params_q, NULL);
  params->LRU_nh = LRU_init(ccache_params_nh, NULL);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void LIRS_free(cache_t *cache) {
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);
  params->LRU_s->cache_free(params->LRU_s);
  params->LRU_q->cache_free(params->LRU_q);
  params->LRU_nh->cache_free(params->LRU_nh);
  my_free(sizeof(LIRS_params_t), params);
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

static bool LIRS_get(cache_t *cache, const request_t *req) {
  bool res = cache_get_base(cache, req);

  limitStack(cache);

#ifdef DEBUG_MODE
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);
  cache_t *LRU_s = params->LRU_s;
  if (cache->n_req >= 2) {
    // printf("obj:%lu size:%ld ", req->obj_id, req->obj_size);
    // if (res) {
    //   printf("hit\n");
    // } else {
    //   printf("miss\n");
    // }
    // LIRS_print_cache(cache);
    LIRS_print_cache_compared_to_cacheus(cache);

    printf("number of requests:%ld \n", cache->n_req);
    printf("number of objects in S:%ld \n", LRU_s->n_obj);
    printf("S(%ld):%ld %ld\n", params->lirs_limit, LRU_s->occupied_byte,
           params->lirs_count);
    printf("Q(%ld): %ld %ld \n", params->hirs_limit,
           params->LRU_q->occupied_byte, params->hirs_count);
    printf("NH: %ld %ld \n", params->LRU_nh->occupied_byte,
           params->nonresident);
    printf("\n\n");
  }
#endif

  return res;
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
static cache_obj_t *LIRS_find(cache_t *cache, const request_t *req,
                              const bool update_cache) {
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);

  cache_obj_t *obj_s = NULL;
  cache_obj_t *obj_q = NULL;

  // find the object in S and Q,
  // and they will be promoted to the top of the stacks
  obj_s = params->LRU_s->find(params->LRU_s, req, update_cache);
  obj_q = params->LRU_q->find(params->LRU_q, req, update_cache);

  if (update_cache == false) {
    if (obj_s != NULL) {
      if (obj_s->LIRS.is_LIR || obj_s->LIRS.in_cache) {
        return obj_s;
      } else {
        return NULL;
      }
    } else if (obj_q != NULL) {
      if (obj_q->LIRS.in_cache) {
        return obj_q;
      } else {
        return NULL;
      }
    } else {
      return NULL;
    }
  }

  // cache_obj_t *res = NULL;
  if (obj_s != NULL) {
    if (obj_s->LIRS.is_LIR) {
      // accessing an LIR block (hit)
      LIRS_prune(cache);
      return obj_s;
    } else {
      // accessing an HIR block in S (resident and non-resident)
      if (obj_s->LIRS.in_cache) {
        return hit_RD_HIRinS(cache, obj_s, obj_q);  // hit
      } else {
        return NULL;
      }
    }
  } else if (obj_q != NULL) {
    // accessing an HIR blocks in Q (resident)
    // LRU_q->find already did the movement
    if (obj_q->LIRS.in_cache) {
      return hit_RD_HIRinQ(cache, obj_q);  // hit
    } else {
      assert(false);
      return NULL;
    }
  } else {
    return NULL;  // miss
  }
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

static cache_obj_t *LIRS_insert(cache_t *cache, const request_t *req) {
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);

  cache_obj_t *obj_s = params->LRU_s->find(params->LRU_s, req, false);
  cache_obj_t *obj_q = params->LRU_q->find(params->LRU_q, req, false);

  // Upon accessing an HIR non-resident in S
  if (obj_s != NULL && obj_s->LIRS.is_LIR == false &&
      obj_s->LIRS.in_cache == false) {
    // change status of the block to be LIR (obj_s is already promoted to top)
    obj_s->LIRS.is_LIR = true;
    obj_s->LIRS.in_cache = true;
    params->lirs_count += obj_s->obj_size;
    cache->occupied_byte += obj_s->obj_size + cache->obj_md_size;
    cache->n_obj += 1;
  }

  // Upon accessing an HIR non-resident in Q
  if (obj_s == NULL && obj_q != NULL && obj_q->LIRS.in_cache == false) {
    // insert the req into S and place it on the top of S
    cache_obj_t *inserted_obj_s = params->LRU_s->insert(params->LRU_s, req);
    inserted_obj_s->LIRS.is_LIR = false;
    inserted_obj_s->LIRS.in_cache = true;

    cache_obj_t *inserted_obj_q = params->LRU_q->insert(params->LRU_q, req);
    inserted_obj_q->LIRS.is_LIR = false;
    inserted_obj_q->LIRS.in_cache = true;

    params->hirs_count += inserted_obj_s->obj_size;
    cache->occupied_byte += inserted_obj_s->obj_size + cache->obj_md_size;
    cache->n_obj += 1;
  }

  // Upon accessing blocks neither in S nor Q
  if (obj_s == NULL && obj_q == NULL) {
    // when LIR block set is not full,
    // all reference blocks are given an LIR status
    if (params->lirs_count + req->obj_size <= params->lirs_limit) {
      cache_obj_t *inserted_obj_s = params->LRU_s->insert(params->LRU_s, req);
      inserted_obj_s->LIRS.is_LIR = true;
      inserted_obj_s->LIRS.in_cache = true;
      params->lirs_count += inserted_obj_s->obj_size;
      cache->occupied_byte += inserted_obj_s->obj_size + cache->obj_md_size;
    } else if (params->hirs_count + req->obj_size <= params->hirs_limit) {
      // when LIR block set is full,
      // all reference blocks are given an HIR status
      cache_obj_t *inserted_obj_s = params->LRU_s->insert(params->LRU_s, req);
      inserted_obj_s->LIRS.is_LIR = false;
      inserted_obj_s->LIRS.in_cache = true;

      cache_obj_t *inserted_obj_q = params->LRU_q->insert(params->LRU_q, req);
      inserted_obj_q->LIRS.is_LIR = false;
      inserted_obj_q->LIRS.in_cache = true;

      params->hirs_count += inserted_obj_s->obj_size;
      cache->occupied_byte += inserted_obj_s->obj_size + cache->obj_md_size;
    } else {
      // the circumstance is same as accessing an HIR non-resident not in S
      // insert the req into S and place it on the top of S
      cache_obj_t *inserted_obj_s = params->LRU_s->insert(params->LRU_s, req);
      inserted_obj_s->LIRS.is_LIR = false;
      inserted_obj_s->LIRS.in_cache = true;

      cache_obj_t *inserted_obj_q = params->LRU_q->insert(params->LRU_q, req);
      inserted_obj_q->LIRS.is_LIR = false;
      inserted_obj_q->LIRS.in_cache = true;

      params->hirs_count += inserted_obj_s->obj_size;
      cache->occupied_byte += inserted_obj_s->obj_size + cache->obj_md_size;
    }
    cache->n_obj += 1;
  }
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
static cache_obj_t *LIRS_to_evict(cache_t *cache, const request_t *req) {
  assert(false);
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
static void LIRS_evict(cache_t *cache, const request_t *req) {
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);

  cache_obj_t *obj_s = params->LRU_s->find(params->LRU_s, req, false);
  cache_obj_t *obj_q = params->LRU_q->find(params->LRU_q, req, false);

  // Upon accessing an HIR non-resident in S
  if (obj_s != NULL && obj_s->LIRS.is_LIR == false &&
      obj_s->LIRS.in_cache == false) {
    // remove the HIR resident at the front of Q
    while (params->hirs_count >= params->hirs_limit) {
      evictHIR(cache);
    }

    // move the LIR block in the bottom of S to Q and conduct stack pruning
    evictLIR(cache);
  }

  // Upon accessing an HIR non-resident in Q
  if (obj_s == NULL && obj_q != NULL && obj_q->LIRS.in_cache == false) {
    // remove the HIR resident at the front of Q
    while (params->hirs_count >= params->hirs_limit) {
      evictHIR(cache);
    }
  }

  // Upon accessing blocks neither in S nor Q
  if (obj_s == NULL && obj_q == NULL) {
    if (params->lirs_count + req->obj_size > params->lirs_limit &&
        params->hirs_count + req->obj_size > params->hirs_limit) {
      // when both LIR and HIR block sets are full,
      // the circumstance is same as accessing an HIR non-resident not in S
      // remove the HIR resident at the front of Q
      evictHIR(cache);
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

static bool LIRS_remove(cache_t *cache, obj_id_t obj_id) {
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);

  cache_obj_t *obj_s = hashtable_find_obj_id(params->LRU_s->hashtable, obj_id);
  cache_obj_t *obj_q = hashtable_find_obj_id(params->LRU_q->hashtable, obj_id);

  // object in S stack (or in Q stack)
  if (obj_s != NULL) {
    if (obj_s->LIRS.is_LIR) {
      params->LRU_s->remove(params->LRU_s, obj_id);
      params->lirs_count -= obj_s->obj_size;
      cache->occupied_byte -= obj_s->obj_size;
      cache->n_obj--;
      LIRS_prune(cache);
    } else {
      if (obj_s->LIRS.in_cache) {
        params->LRU_s->remove(params->LRU_s, obj_id);
        params->hirs_count -= obj_s->obj_size;
        cache->occupied_byte -= obj_s->obj_size;
        cache->n_obj--;
      } else {
        params->LRU_s->remove(params->LRU_s, obj_id);
        params->nonresident -= obj_s->obj_size;
      }
      if (obj_q != NULL) {
        params->LRU_q->remove(params->LRU_q, obj_id);
      }
    }
  } else if (obj_q != NULL) {
    // object only in Q stack
    params->LRU_q->remove(params->LRU_q, obj_id);
    if (obj_q->LIRS.in_cache) {
      params->hirs_count -= obj_q->obj_size;
      cache->occupied_byte -= obj_q->obj_size;
      cache->n_obj--;
    } else {
      assert(false);
      return false;
    }
  } else {
    // object neither in S nor Q stack
    return false;
  }

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                         internal functions                    ****
// ****                                                               ****
// ***********************************************************************

/* LIRS cannot insert an object larger than Q stack size.
 * This function also promise there will be enough space for the object
 * to be inserted into the cache.
 */
bool LIRS_can_insert(cache_t *cache, const request_t *req) {
  bool can_insert = cache_can_insert_default(cache, req);
  if (can_insert == false) {
    return false;
  }
  LIRS_params_t *params = (LIRS_params_t *)cache->eviction_params;
  cache_obj_t *obj_s = params->LRU_s->find(params->LRU_s, req, false);
  cache_obj_t *obj_q = params->LRU_q->find(params->LRU_q, req, false);

  // accessing an HIR non-resident in S
  if (obj_s != NULL && obj_s->LIRS.is_LIR == false &&
      obj_s->LIRS.in_cache == false) {
    while (params->lirs_count + obj_s->obj_size > params->lirs_limit) {
      evictLIR(cache);
    }

    bool res = params->LRU_nh->remove(params->LRU_nh, obj_s->obj_id);
    if (res) {
      params->nonresident -= obj_s->obj_size;
    }

    return true;
  }

  // accessing blocks neither in S nor Q
  if (obj_s == NULL && obj_q == NULL) {
    if (req->obj_size > params->lirs_limit ||
        req->obj_size > params->hirs_limit) {
      WARN_ONCE("object size too large\n");
      // printf("request num: %ld\n", cache->n_req);
      return false;
    }
    if (params->lirs_count + req->obj_size > params->lirs_limit &&
        params->hirs_count + req->obj_size > params->hirs_limit) {
      // when both LIR and HIR block sets are full,
      // the circumstance is same as accessing an HIR non-resident not in S
      while (params->hirs_count + req->obj_size > params->hirs_limit) {
        evictHIR(cache);
      }
      return true;
    }
    return true;
  }

  INFO("LIRS_can_insert: should not reach here. n_req = %ld\n",
       (long)cache->n_req);
  abort();
}

static void LIRS_prune(cache_t *cache) {
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);
  cache_t *LRU_s = params->LRU_s;

  LRU_params_t *lru_s_params = (LRU_params_t *)(LRU_s->eviction_params);
  cache_obj_t *obj_to_remove = lru_s_params->q_tail;

  while (obj_to_remove != lru_s_params->q_head) {
    if (obj_to_remove->LIRS.is_LIR) {
      break;
    }

    // remove obj from LRU_nh
    if (obj_to_remove->LIRS.in_cache == false) {
      bool res = params->LRU_nh->remove(params->LRU_nh, obj_to_remove->obj_id);
      if (res) {
        params->nonresident -= obj_to_remove->obj_size;
      }
    }
    // remove the obj from stack S
    LRU_s->evict(LRU_s, NULL);
    obj_to_remove = lru_s_params->q_tail;
  }
}

static cache_obj_t *hit_RD_HIRinS(cache_t *cache, cache_obj_t *cache_obj_s,
                                  cache_obj_t *cache_obj_q) {
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);

  if (cache_obj_q != NULL) {
    params->hirs_count -= cache_obj_q->obj_size;
    params->LRU_q->remove(params->LRU_q, cache_obj_q->obj_id);
    cache->occupied_byte -= cache_obj_q->obj_size;
    cache->n_obj--;
  }

  while (params->lirs_count + cache_obj_s->obj_size > params->lirs_limit) {
    evictLIR(cache);
  }
  cache_obj_s->LIRS.is_LIR = true;
  params->lirs_count += cache_obj_s->obj_size;

  cache->occupied_byte += cache_obj_s->obj_size;
  cache->n_obj++;

  return cache_obj_s;
}

static cache_obj_t *hit_NR_HIRinS(cache_t *cache, cache_obj_t *cache_obj_s) {
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);

  bool res = params->LRU_nh->remove(params->LRU_nh, cache_obj_s->obj_id);
  if (res) {
    params->nonresident -= cache_obj_s->obj_size;
  }
  return NULL;
}

static cache_obj_t *hit_RD_HIRinQ(cache_t *cache, cache_obj_t *cache_obj_q) {
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);

  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }
  copy_cache_obj_to_request(req_local, cache_obj_q);

  while (params->lirs_count + cache_obj_q->obj_size > params->lirs_limit) {
    evictLIR(cache);
  }
  params->LRU_s->insert(params->LRU_s, req_local);
  cache_obj_t *obj_to_update =
      params->LRU_s->find(params->LRU_s, req_local, false);
  obj_to_update->LIRS.is_LIR = false;
  obj_to_update->LIRS.in_cache = true;

  return obj_to_update;
}

static void evictLIR(cache_t *cache) {
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);
  cache_obj_t *obj_to_evict = params->LRU_s->to_evict(params->LRU_s, NULL);

  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }
  copy_cache_obj_to_request(req_local, obj_to_evict);
  params->lirs_count -= obj_to_evict->obj_size;
  params->LRU_s->evict(params->LRU_s, NULL);

  cache->occupied_byte -= (req_local->obj_size + cache->obj_md_size);
  cache->n_obj -= 1;

  if (req_local->obj_size <= params->hirs_limit) {
    while (params->hirs_count + req_local->obj_size > params->hirs_limit) {
      evictHIR(cache);
    }
    params->LRU_q->insert(params->LRU_q, req_local);

    cache_obj_t *obj_to_update =
        params->LRU_q->find(params->LRU_q, req_local, false);
    obj_to_update->LIRS.is_LIR = false;
    obj_to_update->LIRS.in_cache = true;

    params->hirs_count += req_local->obj_size;
    cache->occupied_byte += (req_local->obj_size + cache->obj_md_size);
    cache->n_obj += 1;
  }

  LIRS_prune(cache);
}

static bool evictHIR(cache_t *cache) {
  LIRS_params_t *params = (LIRS_params_t *)(cache->eviction_params);

  cache_obj_t *obj_to_evict = params->LRU_q->to_evict(params->LRU_q, NULL);

  // update the corresponding block in S to be non-resident
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }
  copy_cache_obj_to_request(req_local, obj_to_evict);

  params->hirs_count -= obj_to_evict->obj_size;
  params->LRU_q->evict(params->LRU_q, NULL);

  cache_obj_t *obj_to_update =
      params->LRU_s->find(params->LRU_s, req_local, false);
  if (obj_to_update != NULL) {
    obj_to_update->LIRS.in_cache = false;
    params->LRU_nh->insert(params->LRU_nh, req_local);
    params->nonresident += obj_to_update->obj_size;
  }

  cache->occupied_byte -= (req_local->obj_size + cache->obj_md_size);
  cache->n_obj -= 1;

  return true;
}

static void limitStack(cache_t *cache) {
  LIRS_params_t *params = (LIRS_params_t *)cache->eviction_params;

  while (params->LRU_s->occupied_byte > (2 * cache->cache_size)) {
    cache_obj_t *obj_to_evict = params->LRU_nh->to_evict(params->LRU_nh, NULL);
    if (obj_to_evict) {
      params->nonresident -= obj_to_evict->obj_size;
      params->LRU_s->remove(params->LRU_s, obj_to_evict->obj_id);
      params->LRU_nh->evict(params->LRU_nh, NULL);
    } else {
      break;
    }
  }
}
// ***********************************************************************
// ****                                                               ****
// ****                       debug functions                         ****
// ****                                                               ****
// ***********************************************************************
static void LIRS_print_cache(cache_t *cache) {
  LIRS_params_t *params = (LIRS_params_t *)cache->eviction_params;

  printf("S Stack:  %lu:%lu %lu:%lu \n", (unsigned long)params->lirs_limit,
         (unsigned long)params->lirs_count, (unsigned long)params->hirs_limit,
         (unsigned long)params->hirs_count);
  cache_obj_t *obj = ((LRU_params_t *)params->LRU_s->eviction_params)->q_head;
  while (obj) {
    printf("%ld(%lu, %s, %s)->", (long)obj->obj_id, obj->obj_size,
           obj->LIRS.in_cache ? "R" : "N", obj->LIRS.is_LIR ? "L" : "H");
    obj = obj->queue.next;
  }
  printf("\n");

  printf("Q Stack: \n");
  cache_obj_t *obj_q = ((LRU_params_t *)params->LRU_q->eviction_params)->q_head;
  while (obj_q) {
    printf("%ld(%lu, %s, %s)->", (long)obj_q->obj_id, obj_q->obj_size,
           obj_q->LIRS.in_cache ? "R" : "N", obj_q->LIRS.is_LIR ? "L" : "H");
    obj_q = obj_q->queue.next;
  }
  printf("\n");

  printf("NH Stack: \n");
  cache_obj_t *obj_nh =
      ((LRU_params_t *)params->LRU_nh->eviction_params)->q_head;
  while (obj_nh) {
    printf("%ld(%lu, %s, %s)->", (long)obj_nh->obj_id, obj_nh->obj_size,
           obj_nh->LIRS.in_cache ? "R" : "N", obj_nh->LIRS.is_LIR ? "L" : "H");
    obj_nh = obj_nh->queue.next;
  }
  printf("\n\n");
}

static void LIRS_print_cache_compared_to_cacheus(cache_t *cache) {
  LIRS_params_t *params = (LIRS_params_t *)cache->eviction_params;

  printf("S:\n");
  cache_obj_t *obj = ((LRU_params_t *)params->LRU_s->eviction_params)->q_tail;
  while (obj) {
    printf("(o=%ld, is_LIR=%s, in_cache=%s)\n", (long)obj->obj_id,
           obj->LIRS.is_LIR ? "True" : "False",
           obj->LIRS.in_cache ? "True" : "False");
    obj = obj->queue.prev;
  }

  printf("Q:\n");
  cache_obj_t *obj_q = ((LRU_params_t *)params->LRU_q->eviction_params)->q_tail;
  while (obj_q) {
    printf("(o=%ld, is_LIR=%s, in_cache=%s)\n", (long)obj_q->obj_id,
           obj_q->LIRS.is_LIR ? "True" : "False",
           obj_q->LIRS.in_cache ? "True" : "False");
    obj_q = obj_q->queue.prev;
  }

  printf("NH:\n");
  cache_obj_t *obj_nh =
      ((LRU_params_t *)params->LRU_nh->eviction_params)->q_tail;
  while (obj_nh) {
    printf("(o=%ld, is_LIR=%s, in_cache=%s)\n", (long)obj_nh->obj_id,
           obj_nh->LIRS.is_LIR ? "True" : "False",
           obj_nh->LIRS.in_cache ? "True" : "False");
    obj_nh = obj_nh->queue.prev;
  }
  printf("\n");
}

#ifdef __cplusplus
}
#endif
