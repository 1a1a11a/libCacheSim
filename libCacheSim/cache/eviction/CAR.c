//
//  CAR, the same as FIFO-Reinsertion or second chance, is a fusion
//  of CLOCK with Adapative Replacement Cache
//
//  Created by Rafly Hanggaraksa
//
//

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

// #define DEBUG_MODE

typedef struct {
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
  int64_t last_req_in_ghost;
} CAR_params_t;

static void CAR_parse_params(cache_t *cache, const char *cache_specific_params);
static void CAR_free(cache_t *cache);
static bool CAR_get(cache_t *cache, const request_t *req);
static cache_obj_t *CAR_find(cache_t *cache, const request_t *req,
                             const bool update_cache);
static cache_obj_t *CAR_insert(cache_t *cache, const request_t *req);
static cache_obj_t *CAR_to_evict(cache_t *cache, const request_t *req);
static void CAR_evict(cache_t *cache, const request_t *req);
static bool CAR_remove(cache_t *cache, const obj_id_t obj_id);

static void _CAR_discard_LRU_L1_ghost(cache_t *cache, const request_t *req);
static void _CAR_discard_LRU_L2_ghost(cache_t *cache, const request_t *req);
static void _CAR_L1_move_to_tail_L2_data(cache_t *cache, const request_t *req);
static void _CAR_move_to_tail_L2_data(cache_t *cache, const request_t *req);
static void _CAR_L2_demote_to_MRU_data(cache_t *cache, const request_t *req);
static void _CAR_L1_demote_to_MRU_data(cache_t *cache, const request_t *req);
static cache_obj_t *_CAR_to_replace(cache_t *cache, const request_t *req);
static void _CAR_replace(cache_t *cache, const request_t *req);
static void _CAR_sanity_check(cache_t *cache, const request_t *req);
static void _CAR_sanity_check_full(cache_t *cache, const request_t *req);
static bool _CAR_get_debug(cache_t *cache, const request_t *req);
static void print_cache(cache_t *cache);

static const char *DEFAULT_PARAMS = "p=0";

cache_t *CAR_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("CAR", ccache_params, cache_specific_params);
  cache->cache_init = CAR_init;
  cache->cache_free = CAR_free;
  cache->get = CAR_get;
  cache->find = CAR_find;
  cache->insert = CAR_insert;
  cache->evict = CAR_evict;
  cache->remove = CAR_remove;
  cache->can_insert = cache_can_insert_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->to_evict = CAR_to_evict;
  cache->obj_md_size = 0;

  if (ccache_params.consider_obj_metadata) {
    // two pointer + ghost metadata
    cache->obj_md_size = 8 * 2 + 8 * 3;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = malloc(sizeof(CAR_params_t));
  memset(cache->eviction_params, 0, sizeof(CAR_params_t));
  CAR_params_t *params = (CAR_params_t *)cache->eviction_params;

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
  params->last_req_in_ghost = -1;
  params->p = 0;

  CAR_parse_params(cache, DEFAULT_PARAMS);
  if (cache_specific_params != NULL) {
    CAR_parse_params(cache, cache_specific_params);
  }

  return cache;
}

/**
 * @brief initialize a CAR cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params Clock specific parameters as a string
 */
static cache_obj_t *CAR_find(cache_t *cache, const request_t *req,
                             const bool update_cache) {
  CAR_params_t *params = (CAR_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);

  if (!update_cache) {
    return obj->CAR.ghost ? NULL : obj;
  }

  if (obj == NULL) {
    return NULL;
  }

  cache_obj_t *result = obj;
  if (obj->CAR.ghost) {
    // Object in Ghost
    result = NULL;
    if (obj->CAR.lru_id == 1) {  // Obj in B1
      params->curr_obj_in_L1_ghost = true;
      params->last_req_in_ghost = cache->n_req;
      // Adapt: Increase the target size for the list T1 as: p = min {p + max{1,
      // |B2|/|B1|}, c}
      params->p =
          MIN(params->p + MAX(1, params->L2_ghost_size / params->L1_ghost_size),
              cache->cache_size);
      // Move x at the tail of T2. Set the page reference bit of x to 0.
      remove_obj_from_list(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
      params->L1_ghost_size -= obj->obj_size + cache->obj_md_size;
      hashtable_delete(cache->hashtable, obj);

    } else {  // Obj in B2
      params->curr_obj_in_L2_ghost = true;

      //  Adapt: Decrease the target size for the list T1 as: p = max {p −
      //  max{1, |B1|/|B2|}, 0}
      params->p = MAX(
          params->p - MAX(1, params->L1_ghost_size / params->L2_ghost_size), 0);
      //  Move x at the tail of T2. Set the page reference bit of x to 0.
      remove_obj_from_list(&params->L2_ghost_head, &params->L2_ghost_tail, obj);
      params->L2_ghost_size -= obj->obj_size + cache->obj_md_size;
      hashtable_delete(cache->hashtable, obj);
    }
  } else {
    obj->CAR.reference = true;
  }
  return result;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void CAR_free(cache_t *cache) {
  free(cache->eviction_params);
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
static bool CAR_get(cache_t *cache, const request_t *req) {
#ifdef DEBUG_MODE
  return _CAR_get_debug(cache, req);
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
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * and eviction is not part of this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
static cache_obj_t *CAR_insert(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_insert_base(cache, req);

  if ((params->curr_obj_in_L1_ghost || params->curr_obj_in_L2_ghost)) {
    // Insert at the tail of T2
    obj->CAR.lru_id = 2;
    obj->CAR.reference = false;  // Set the page reference bit to 0
    append_obj_to_tail(&params->L2_data_head, &params->L2_data_tail, obj);
    params->L2_data_size += req->obj_size + cache->obj_md_size;

    params->curr_obj_in_L1_ghost = false;
    params->curr_obj_in_L2_ghost = false;
  } else {
    // Insert at the tail of T1
    obj->CAR.lru_id = 1;
    obj->CAR.reference = false;  // Set the page reference bit to 0
    append_obj_to_tail(&params->L1_data_head, &params->L1_data_tail, obj);
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
static cache_obj_t *CAR_to_evict(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)(cache->eviction_params);
  if (params->L1_data_size + params->L2_data_size >= cache->cache_size) {
    // Cache full, replace a page from cache
    cache->to_evict_candidate = _CAR_to_replace(cache, req);
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
static void CAR_evict(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)(cache->eviction_params);
  int64_t incoming_size = req->obj_size + cache->obj_md_size;
  if ((params->L1_data_size + params->L2_data_size + incoming_size >=
       cache->cache_size)) {
    _CAR_replace(cache, req);
    if ((!params->curr_obj_in_L1_ghost || !params->curr_obj_in_L2_ghost)) {
      if ((params->L1_data_size + params->L1_ghost_size >= cache->cache_size)) {
        _CAR_discard_LRU_L1_ghost(cache, req);
      } else if ((params->L1_data_size + params->L1_ghost_size >
                  cache->cache_size) &&
                 (params->L1_data_size + params->L2_data_size +
                      params->L1_ghost_size + params->L2_ghost_size +
                      incoming_size >=
                  cache->cache_size * 2)) {
        _CAR_discard_LRU_L2_ghost(cache, req);
      }
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
static bool CAR_remove(cache_t *cache, const obj_id_t obj_id) {
  CAR_params_t *params = (CAR_params_t *)(cache->eviction_params);
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);

  if (obj == NULL) {
    return false;
  }

  if (obj->CAR.ghost) {
    if (obj->CAR.lru_id == 1) {
      params->L1_ghost_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
    } else {
      params->L2_ghost_size -= obj->obj_size + cache->obj_md_size;
      remove_obj_from_list(&params->L2_ghost_head, &params->L2_ghost_tail, obj);
    }
  } else {
    if (obj->CAR.lru_id == 1) {
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

static void _CAR_replace(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)(cache->eviction_params);
  // _CAR_sanity_check(cache, req);
  bool found = false;
  while (!found) {
    if (params->L1_data_size >= MAX(1, params->p)) {
      if (!params->L1_data_head->CAR.reference) {
        found = true;
        _CAR_L1_demote_to_MRU_data(cache, req);
      } else {
        params->L1_data_head->CAR.reference = false;
        _CAR_L1_move_to_tail_L2_data(cache, req);
      }
    } else {
      if (!params->L2_data_head->CAR.reference) {
        found = true;
        _CAR_L2_demote_to_MRU_data(cache, req);

      } else {
        params->L2_data_head->CAR.reference = 0;
        _CAR_move_to_tail_L2_data(cache, req);
      }
    }
  }
}

static cache_obj_t *_CAR_to_replace(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)(cache->eviction_params);
  cache_obj_t *obj = NULL;

  bool found = false;
  while (!found) {
    if (params->L1_data_size >= MAX(1, params->p)) {
      if (!params->L1_data_head->CAR.reference) {
        found = true;
        obj = params->L1_data_head;
      }
    } else {
      if (!params->L2_data_head->CAR.reference) {
        found = true;
        obj = params->L2_data_head;
      }
    }
  }

  return obj;
}

static void _CAR_L1_demote_to_MRU_data(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)cache->eviction_params;
  cache_obj_t *obj = params->L1_data_head;

  remove_obj_from_list(&params->L1_data_head, &params->L1_data_tail, obj);
  params->L1_data_size -= obj->obj_size + cache->obj_md_size;
  params->L1_ghost_size += obj->obj_size + cache->obj_md_size;
  prepend_obj_to_head(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
  obj->CAR.ghost = true;

  cache_evict_base(cache, obj, false);
}

static void _CAR_L2_demote_to_MRU_data(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)cache->eviction_params;
  cache_obj_t *obj = params->L2_data_head;

  remove_obj_from_list(&params->L2_data_head, &params->L2_data_tail, obj);
  params->L2_data_size -= obj->obj_size + cache->obj_md_size;
  params->L2_ghost_size += obj->obj_size + cache->obj_md_size;
  prepend_obj_to_head(&params->L2_ghost_head, &params->L2_ghost_tail, obj);
  obj->CAR.ghost = true;

  cache_evict_base(cache, obj, false);
}

static void _CAR_L1_move_to_tail_L2_data(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)cache->eviction_params;
  cache_obj_t *obj = params->L1_data_head;
  remove_obj_from_list(&params->L1_data_head, &params->L1_data_tail, obj);
  params->L1_data_size -= obj->obj_size + cache->obj_md_size;
  params->L2_data_size += obj->obj_size + cache->obj_md_size;
  append_obj_to_tail(&params->L2_data_head, &params->L2_data_tail, obj);
  obj->CAR.ghost = false;
  obj->CAR.lru_id = 2;
}

static void _CAR_move_to_tail_L2_data(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)cache->eviction_params;
  cache_obj_t *obj = params->L2_data_head;

  move_obj_to_tail(&params->L2_data_head, &params->L2_data_tail, obj);
  obj->CAR.ghost = false;
}

static void _CAR_discard_LRU_L1_ghost(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)cache->eviction_params;
  cache_obj_t *obj = params->L1_ghost_tail;
  int64_t sz = obj->obj_size + cache->obj_md_size;
  params->L1_ghost_size -= sz;
  remove_obj_from_list(&params->L1_ghost_head, &params->L1_ghost_tail, obj);
  hashtable_delete(cache->hashtable, obj);
}

static void _CAR_discard_LRU_L2_ghost(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)cache->eviction_params;
  cache_obj_t *obj = params->L2_ghost_tail;
  int64_t sz = obj->obj_size + cache->obj_md_size;
  params->L2_ghost_size -= sz;
  remove_obj_from_list(&params->L2_ghost_head, &params->L2_ghost_tail, obj);
  hashtable_delete(cache->hashtable, obj);
}

// ***********************************************************************
// ****                                                               ****
// ****                  parameter set up functions                   ****
// ****                                                               ****
// ***********************************************************************
static const char *CAR_current_params(cache_t *cache, CAR_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "p=%f\n", params->p);

  return params_str;
}

static void CAR_parse_params(cache_t *cache,
                             const char *cache_specific_params) {
  CAR_params_t *params = (CAR_params_t *)cache->eviction_params;
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

    if (strcasecmp(key, "p") == 0) {
      params->p = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", CAR_current_params(cache, params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s, example parameters %s\n",
            cache->cache_name, key, CAR_current_params(cache, params));
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
  CAR_params_t *params = (CAR_params_t *)(cache->eviction_params);

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

static void _CAR_sanity_check(cache_t *cache, const request_t *req) {
  CAR_params_t *params = (CAR_params_t *)(cache->eviction_params);

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

static inline void _CAR_sanity_check_full(cache_t *cache,
                                          const request_t *req) {
  // if (cache->n_req < 13200000) return;

  _CAR_sanity_check(cache, req);

  CAR_params_t *params = (CAR_params_t *)(cache->eviction_params);

  int64_t L1_data_byte = 0, L2_data_byte = 0;
  int64_t L1_ghost_byte = 0, L2_ghost_byte = 0;

  cache_obj_t *obj = params->L1_data_head;
  cache_obj_t *last_obj = NULL;
  while (obj != NULL) {
    DEBUG_ASSERT(obj->CAR.lru_id == 1);
    DEBUG_ASSERT(!obj->CAR.ghost);
    L1_data_byte += obj->obj_size;
    last_obj = obj;
    obj = obj->queue.next;
  }
  DEBUG_ASSERT(L1_data_byte == params->L1_data_size);
  DEBUG_ASSERT(last_obj == params->L1_data_tail);

  obj = params->L1_ghost_head;
  last_obj = NULL;
  while (obj != NULL) {
    DEBUG_ASSERT(obj->CAR.lru_id == 1);
    DEBUG_ASSERT(obj->CAR.ghost);
    L1_ghost_byte += obj->obj_size;
    last_obj = obj;
    obj = obj->queue.next;
  }
  DEBUG_ASSERT(L1_ghost_byte == params->L1_ghost_size);
  DEBUG_ASSERT(last_obj == params->L1_ghost_tail);

  obj = params->L2_data_head;
  last_obj = NULL;
  while (obj != NULL) {
    DEBUG_ASSERT(obj->CAR.lru_id == 2);
    DEBUG_ASSERT(!obj->CAR.ghost);
    L2_data_byte += obj->obj_size;
    last_obj = obj;
    obj = obj->queue.next;
  }
  DEBUG_ASSERT(L2_data_byte == params->L2_data_size);
  DEBUG_ASSERT(last_obj == params->L2_data_tail);

  obj = params->L2_ghost_head;
  last_obj = NULL;
  while (obj != NULL) {
    DEBUG_ASSERT(obj->CAR.lru_id == 2);
    DEBUG_ASSERT(obj->CAR.ghost);
    L2_ghost_byte += obj->obj_size;
    last_obj = obj;
    obj = obj->queue.next;
  }
  DEBUG_ASSERT(L2_ghost_byte == params->L2_ghost_size);
  DEBUG_ASSERT(last_obj == params->L2_ghost_tail);
}

static bool _CAR_get_debug(cache_t *cache, const request_t *req) {
  cache->n_req += 1;

  cache_obj_t *obj = cache->find(cache, req, true);

  _CAR_sanity_check_full(cache, req);

  if (obj != NULL) {
    return true;
  }

  if (!cache->can_insert(cache, req)) {
    return false;
  }

  while (cache->occupied_byte + req->obj_size + cache->obj_md_size >
         cache->cache_size) {
    cache->evict(cache, req);
  }

  _CAR_sanity_check_full(cache, req);

  cache->insert(cache, req);
  _CAR_sanity_check_full(cache, req);

  return false;
}
