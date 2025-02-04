//
//  a LRU module that supports different obj size
//
//  SLRUv0.c
//  libCacheSim
//
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SLRUv0_params {
  cache_t **LRUs;
  int n_seg;
  // a temporary request used to move object between LRUs
  request_t *req_local;
} SLRUv0_params_t;


static const char *DEFAULT_CACHE_PARAMS = "n-seg=4";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void SLRUv0_parse_params(cache_t *cache,
                                const char *cache_specific_params);
static void SLRUv0_free(cache_t *cache);
static bool SLRUv0_get(cache_t *cache, const request_t *req);
static cache_obj_t *SLRUv0_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *SLRUv0_insert(cache_t *cache, const request_t *req);
static cache_obj_t *SLRUv0_to_evict(cache_t *cache, const request_t *req);
static void SLRUv0_evict(cache_t *cache, const request_t *req);
static bool SLRUv0_remove(cache_t *cache, const obj_id_t obj_id);
static void SLRUv0_cool(cache_t *cache, const request_t *req, int i);

/* SLRUv0 cannot an object larger than segment size */
static inline bool SLRUv0_can_insert(cache_t *cache, const request_t *req);
static inline int64_t SLRUv0_get_occupied_byte(const cache_t *cache);
static inline int64_t SLRUv0_get_n_obj(const cache_t *cache);

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
cache_t *SLRUv0_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("SLRUv0", ccache_params, cache_specific_params);
  cache->cache_init = SLRUv0_init;
  cache->cache_free = SLRUv0_free;
  cache->get = SLRUv0_get;
  cache->find = SLRUv0_find;
  cache->insert = SLRUv0_insert;
  cache->evict = SLRUv0_evict;
  cache->remove = SLRUv0_remove;
  cache->to_evict = SLRUv0_to_evict;
  cache->can_insert = SLRUv0_can_insert;
  cache->get_occupied_byte = SLRUv0_get_occupied_byte;
  cache->get_n_obj = SLRUv0_get_n_obj;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = (SLRUv0_params_t *)malloc(sizeof(SLRUv0_params_t));
  SLRUv0_params_t *params = (SLRUv0_params_t *)(cache->eviction_params);

  SLRUv0_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    SLRUv0_parse_params(cache, cache_specific_params);
  }

  params->LRUs = (cache_t **)malloc(sizeof(cache_t *) * params->n_seg);

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size /= params->n_seg;
  ccache_params_local.hashpower = MIN(16, ccache_params_local.hashpower - 4);
  params->LRUs[0] = LRU_init(ccache_params_local, NULL);
  for (int i = 1; i < params->n_seg; i++) {
    params->LRUs[i] = LRU_init(ccache_params_local, NULL);
  }
  params->req_local = new_request();

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void SLRUv0_free(cache_t *cache) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)(cache->eviction_params);
  free_request(params->req_local);
  for (int i = 0; i < params->n_seg; i++)
    params->LRUs[i]->cache_free(params->LRUs[i]);
  free(params->LRUs);
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
static bool SLRUv0_get(cache_t *cache, const request_t *req) {
  /* because this field cannot be updated in time since segment LRUs are
   * updated, so we should not use this field */
  DEBUG_ASSERT(cache->occupied_byte == 0);

  bool ck = cache_get_base(cache, req);

  return ck;
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
static cache_obj_t *SLRUv0_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)(cache->eviction_params);

  cache_obj_t *obj = NULL;
  for (int i = 0; i < params->n_seg; i++) {
    obj = params->LRUs[i]->find(params->LRUs[i], req, false);
    bool cache_hit = obj != NULL;

    if (obj == NULL) {
      continue;
    }

    if (!update_cache) {
      return obj;
    }

    if (cache_hit && i != params->n_seg - 1) {
      // bump object from lower segment to upper segment;
      int src_id = i;
      int dest_id = i + 1;
      if (i == params->n_seg - 1) {
        dest_id = i;
      }

      params->LRUs[src_id]->remove(params->LRUs[src_id], req->obj_id);
      assert(params->LRUs[dest_id]->find(params->LRUs[dest_id], req, false) ==
             NULL);

      // If the upper LRU is full;
      while (params->LRUs[dest_id]->occupied_byte + req->obj_size +
                 cache->obj_md_size >
             params->LRUs[dest_id]->cache_size)
        SLRUv0_cool(cache, req, dest_id);

      params->LRUs[dest_id]->insert(params->LRUs[dest_id], req);
    }

    return obj;
  }

  return NULL;
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
static cache_obj_t *SLRUv0_insert(cache_t *cache, const request_t *req) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)(cache->eviction_params);

  // this is used by eviction age tracking
  params->LRUs[0]->n_req = cache->n_req;

  // Find the lowest LRU with space for insertion
  for (int i = 0; i < params->n_seg; i++) {
    cache_t *lru = params->LRUs[i];
    if (lru->get_occupied_byte(lru) + req->obj_size + cache->obj_md_size <=
        lru->cache_size) {
      return lru->insert(lru, req);
    }
  }

  // If all LRUs are filled, evict an obj from the lowest LRU.
  cache_t *lru = params->LRUs[0];
  while (lru->get_occupied_byte(lru) + req->obj_size + cache->obj_md_size >
         lru->cache_size) {
    SLRUv0_evict(cache, req);
  }
  return lru->insert(lru, req);
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
static cache_obj_t *SLRUv0_to_evict(cache_t *cache, const request_t *req) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)(cache->eviction_params);
  for (int i = 0; i < params->n_seg; i++) {
    cache_t *lru = params->LRUs[i];
    if (lru->get_occupied_byte(lru) > 0) {
      return lru->to_evict(lru, req);
    }
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
 * @param evicted_obj if not NULL, return the evicted object to caller
 */
static void SLRUv0_evict(cache_t *cache, const request_t *req) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)(cache->eviction_params);

  int nth_seg_to_evict = 0;
  for (int i = 0; i < params->n_seg; i++) {
    cache_t *lru = params->LRUs[i];
    if (lru->get_occupied_byte(lru) > 0) {
      nth_seg_to_evict = i;
      break;
    }
  }

  cache_t *lru = params->LRUs[nth_seg_to_evict];
  lru->evict(lru, req);
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
static bool SLRUv0_remove(cache_t *cache, const obj_id_t obj_id) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)(cache->eviction_params);
  for (int i = 0; i < params->n_seg; i++) {
    cache_t *lru = params->LRUs[i];
    if (lru->remove(lru, obj_id)) {
      return true;
    }
  }
  return false;
}

/* SLRUv0 cannot an object larger than segment size */
static inline bool SLRUv0_can_insert(cache_t *cache, const request_t *req) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);
  return can_insert &&
         (req->obj_size + cache->obj_md_size <= params->LRUs[0]->cache_size);
}

static inline int64_t SLRUv0_get_occupied_byte(const cache_t *cache) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)cache->eviction_params;
  int64_t occupied_byte = 0;
  for (int i = 0; i < params->n_seg; i++) {
    occupied_byte += params->LRUs[i]->get_occupied_byte(params->LRUs[i]);
  }
  return occupied_byte;
}

static inline int64_t SLRUv0_get_n_obj(const cache_t *cache) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)cache->eviction_params;
  int64_t n_obj = 0;
  for (int i = 0; i < params->n_seg; i++) {
    n_obj += params->LRUs[i]->get_n_obj(params->LRUs[i]);
  }
  return n_obj;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *SLRUv0_current_params(SLRUv0_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-seg=%d\n", params->n_seg);
  return params_str;
}

static void SLRUv0_parse_params(cache_t *cache,
                                const char *cache_specific_params) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)cache->eviction_params;
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

    if (strcasecmp(key, "n-seg") == 0) {
      params->n_seg = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }

    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", SLRUv0_current_params(params));
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
// ****              cache internal functions                         ****
// ****                                                               ****
// ***********************************************************************
/**
 * @brief move an object from ith LRU into (i-1)th LRU, cool
 * (i-1)th LRU if it is full
 *
 * @param cache
 * @param i
 */
static void SLRUv0_cool(cache_t *cache, const request_t *req, int i) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)(cache->eviction_params);
  request_t *saved_req = new_request();
  cache_t *lru = params->LRUs[i];
  // the last LRU is evict-only, do not move to a lower lru
  if (i == 0) {
    lru->evict(lru, NULL);
    return;
  };

  // the evicted object move to lower lru
  cache_obj_t *obj_evicted = lru->to_evict(lru, req);
  copy_cache_obj_to_request(saved_req, obj_evicted);
  lru->evict(lru, NULL);

  // If lower LRUs are full
  cache_t *next_lru = params->LRUs[i - 1];
  int64_t required_size =
      next_lru->cache_size - saved_req->obj_size - cache->obj_md_size;
  while (next_lru->get_occupied_byte(next_lru) > required_size) {
    SLRUv0_cool(cache, req, i - 1);
  }

  next_lru->insert(next_lru, saved_req);
  free_request(saved_req);
}

// ***********************************************************************
// ****                                                               ****
// ****                       debug functions                         ****
// ****                                                               ****
// ***********************************************************************
static void SLRUv0_print_cache(cache_t *cache) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)cache->eviction_params;
  for (int i = params->n_seg - 1; i >= 0; i--) {
    cache_obj_t *obj =
        ((LRU_params_t *)params->LRUs[i]->eviction_params)->q_head;
    while (obj) {
      printf("%ld(%lu)->", (long) obj->obj_id, obj->obj_size);
      obj = obj->queue.next;
    }
    printf(" | ");
  }
  printf("\n");
}

#ifdef __cplusplus
extern "C"
}
#endif
