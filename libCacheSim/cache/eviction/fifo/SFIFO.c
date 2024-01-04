//
//  segmented fifo implemented using multiple lists instead of multiple fifos
//  this has a better performance than SFIFOv0, but it is very hard to implement
//
//  SFIFO.c
//  libCacheSim
//
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define DEBUG_MODE

// ***********************************************************************
// ****                                                               ****
// ****                    debug macros                               ****
// ****                                                               ****
// ***********************************************************************
#ifdef DEBUG_MODE
#define DEBUG_PRINT_CACHE_STATE(cache, params, req)                   \
  do {                                                                \
    if (cache->n_req > 0) {                                          \
      printf("%ld %ld %s: %s: ", cache->n_req, req->obj_id, __func__, \
             (char *)cache->last_request_metadata);                   \
      for (int i = 0; i < params->n_seg; i++) {                       \
        printf("%ld/%ld/%p/%p, ", params->fifo_n_objs[i],             \
               params->fifo_n_bytes[i], params->fifo_heads[i],        \
               params->fifo_tails[i]);                                \
      }                                                               \
      printf("\n");                                                   \
      _SFIFO_verify_fifo_size(cache);                                 \
    }                                                                 \
  } while (0)

#define DEBUG_PRINT_CACHE(cache, params)                 \
  do {                                                   \
    for (int i = params->n_seg - 1; i >= 0; i--) {       \
      cache_obj_t *obj = params->fifo_heads[i];          \
      while (obj != NULL) {                              \
        printf("%lu(%u)->", obj->obj_id, obj->obj_size); \
        obj = obj->queue.next;                           \
      }                                                  \
      printf(" | ");                                     \
    }                                                    \
    printf("\n");                                        \
  } while (0)
#else
#define DEBUG_PRINT_CACHE_STATE(cache, params, req)
#define DEBUG_PRINT_CACHE(cache, params)
#endif

typedef struct SFIFO_params {
  cache_obj_t **fifo_heads;
  cache_obj_t **fifo_tails;
  int64_t *fifo_n_bytes;
  int64_t *fifo_n_objs;
  int64_t per_seg_max_size;
  int n_seg;
} SFIFO_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
static void SFIFO_parse_params(cache_t *cache,
                               const char *cache_specific_params);
static void SFIFO_parse_params(cache_t *cache, const char *cache_specific_params);
static void SFIFO_free(cache_t *cache);
static bool SFIFO_get(cache_t *cache, const request_t *req);
static cache_obj_t *SFIFO_find(cache_t *cache, const request_t *req,
                             const bool update_cache);
static cache_obj_t *SFIFO_insert(cache_t *cache, const request_t *req);
static cache_obj_t *SFIFO_to_evict(cache_t *cache, const request_t *req);
static void SFIFO_evict(cache_t *cache, const request_t *req);
static bool SFIFO_remove(cache_t *cache, const obj_id_t obj_id);

/* internal functions */
static bool SFIFO_can_insert(cache_t *cache, const request_t *req);
static void SFIFO_promote_to_next_seg(cache_t *cache, const request_t *req,
                                      cache_obj_t *obj);
static void SFIFO_demote_to_prev_seg(cache_t *cache, const request_t *req,
                                     cache_obj_t *obj);
static void SFIFO_cool(cache_t *cache, const request_t *req, const int id);

/* debug functions */
static void _SFIFO_verify_fifo_size(cache_t *cache);
bool SFIFO_get_debug(cache_t *cache, const request_t *req);

static inline bool seg_too_large(cache_t *cache, int seg_id) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  return params->fifo_n_bytes[seg_id] > params->per_seg_max_size;
}

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************
static void SFIFO_free(cache_t *cache) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  free(params->fifo_heads);
  free(params->fifo_tails);
  free(params->fifo_n_objs);
  free(params->fifo_n_bytes);
  cache_struct_free(cache);
}

/**
 * @brief initialize a SFIFO cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params SFIFO specific parameters, e.g., "n-seg=4"
 */
cache_t *SFIFO_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("SFIFO", ccache_params, cache_specific_params);
  cache->cache_init = SFIFO_init;
  cache->cache_free = SFIFO_free;
  cache->get = SFIFO_get;
  cache->find = SFIFO_find;
  cache->insert = SFIFO_insert;
  cache->evict = SFIFO_evict;
  cache->remove = SFIFO_remove;
  cache->to_evict = SFIFO_to_evict;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->can_insert = SFIFO_can_insert;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = (SFIFO_params_t *)malloc(sizeof(SFIFO_params_t));
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  params->n_seg = 4;

  if (cache_specific_params != NULL) {
    SFIFO_parse_params(cache, cache_specific_params);
  }

  params->per_seg_max_size = ccache_params.cache_size / params->n_seg;
  params->fifo_heads =
      (cache_obj_t **)malloc(sizeof(cache_obj_t *) * params->n_seg);
  params->fifo_tails =
      (cache_obj_t **)malloc(sizeof(cache_obj_t *) * params->n_seg);
  params->fifo_n_objs = (int64_t *)malloc(sizeof(int64_t) * params->n_seg);
  params->fifo_n_bytes = (int64_t *)malloc(sizeof(int64_t) * params->n_seg);

  for (int i = 0; i < params->n_seg; i++) {
    params->fifo_heads[i] = NULL;
    params->fifo_tails[i] = NULL;
    params->fifo_n_objs[i] = 0;
    params->fifo_n_bytes[i] = 0;
  }

  return cache;
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
static bool SFIFO_get(cache_t *cache, const request_t *req) {
#ifdef DEBUG_MODE
  return SFIFO_get_debug(cache, req);
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
 * @brief find an object in the cache
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return the object or NULL if not found
 */
static cache_obj_t *SFIFO_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  cache_obj_t *obj = hashtable_find(cache->hashtable, req);

  if (obj == NULL || !update_cache) {
    return obj;
  }

  obj->SFIFO.freq++;
  SFIFO_promote_to_next_seg(cache, req, obj);
  while (params->fifo_n_bytes[obj->SFIFO.fifo_id] > params->per_seg_max_size) {
    SFIFO_cool(cache, req, obj->SFIFO.fifo_id);
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
cache_obj_t *SFIFO_insert(cache_t *cache, const request_t *req) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  // Find the lowest fifo with space for insertion
  int nth_seg = -1;
  for (int i = 0; i < params->n_seg; i++) {
    if (params->fifo_n_bytes[i] + req->obj_size + cache->obj_md_size <=
        params->per_seg_max_size) {
      nth_seg = i;
      break;
    }
  }

  if (nth_seg == -1) {
    // No space for insertion
    while (cache->occupied_byte + req->obj_size + cache->obj_md_size >
           cache->cache_size) {
      cache->evict(cache, req);
    }
    nth_seg = 0;
  }

  // cache_obj_t *obj = hashtable_insert(cache->hashtable, req);
  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->SFIFO.freq = 0;
  obj->SFIFO.fifo_id = nth_seg;
  obj->SFIFO.last_access_vtime = cache->n_req;

  prepend_obj_to_head(&params->fifo_heads[nth_seg],
                      &params->fifo_tails[nth_seg], obj);
  params->fifo_n_bytes[nth_seg] += req->obj_size + cache->obj_md_size;
  params->fifo_n_objs[nth_seg]++;

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
static cache_obj_t *SFIFO_to_evict(cache_t *cache, const request_t *req) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  for (int i = 0; i < params->n_seg; i++) {
    if (params->fifo_n_bytes[i] > 0) {
      return params->fifo_tails[i];
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
static void SFIFO_evict(cache_t *cache, const request_t *req) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  int nth_seg = -1;
  for (int i = 0; i < params->n_seg; i++) {
    if (params->fifo_tails[i] != NULL) {
      nth_seg = i;
      break;
    }
  }

  cache_obj_t *obj = params->fifo_tails[nth_seg];
  DEBUG_ASSERT(obj != NULL);

  params->fifo_n_bytes[nth_seg] -= obj->obj_size + cache->obj_md_size;
  params->fifo_n_objs[nth_seg]--;

  remove_obj_from_list(&params->fifo_heads[nth_seg],
                       &params->fifo_tails[nth_seg], obj);
  cache_evict_base(cache, obj, true);
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
static bool SFIFO_remove(cache_t *cache, const obj_id_t obj_id) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);

  if (obj == NULL) {
    return false;
  }

  cache->occupied_byte -= (obj->obj_size + cache->obj_md_size);
  cache->n_obj -= 1;
  remove_obj_from_list(&(params->fifo_heads[obj->SFIFO.fifo_id]),
                       &(params->fifo_tails[obj->SFIFO.fifo_id]), obj);
  hashtable_delete(cache->hashtable, obj);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                     setup functions                           ****
// ****                                                               ****
// ***********************************************************************
static const char *SFIFO_current_params(SFIFO_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-seg=%d\n", params->n_seg);
  return params_str;
}

static void SFIFO_parse_params(cache_t *cache,
                               const char *cache_specific_params) {
  SFIFO_params_t *params = (SFIFO_params_t *)cache->eviction_params;
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
      printf("current parameters: %s\n", SFIFO_current_params(params));
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
// ****                   internal functions                          ****
// ****                                                               ****
// ***********************************************************************
/* SFIFO cannot insert an object larger than segment size */
bool SFIFO_can_insert(cache_t *cache, const request_t *req) {
  SFIFO_params_t *params = (SFIFO_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);
  return can_insert &&
         (req->obj_size + cache->obj_md_size <= params->per_seg_max_size);
}

/**
 * @brief move an object from ith fifo into (i-1)th fifo, cool
 * (i-1)th fifo if it is full, where the n_seg th fifo is the most recent
 *
 * @param cache
 * @param i
 */
static void SFIFO_cool(cache_t *cache, const request_t *req, const int id) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  if (id == 0) return SFIFO_evict(cache, req);

  cache_obj_t *obj = params->fifo_tails[id];
  DEBUG_ASSERT(obj != NULL && obj->SFIFO.fifo_id == id);
  remove_obj_from_list(&params->fifo_heads[id], &params->fifo_tails[id], obj);
  prepend_obj_to_head(&params->fifo_heads[id - 1], &params->fifo_tails[id - 1],
                      obj);
  obj->SFIFO.fifo_id = id - 1;
  obj->SFIFO.freq = 0;
  params->fifo_n_bytes[id] -= obj->obj_size;
  params->fifo_n_objs[id]--;
  params->fifo_n_bytes[id - 1] += obj->obj_size;
  params->fifo_n_objs[id - 1]++;

  // If lower fifos are full
  while (params->fifo_n_bytes[id - 1] > params->per_seg_max_size) {
    SFIFO_cool(cache, req, id - 1);
  }
}

static void SFIFO_demote_to_prev_seg(cache_t *cache, const request_t *req,
                                     cache_obj_t *obj) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  assert(obj->SFIFO.fifo_id > 0);
  DEBUG_ASSERT(obj->SFIFO.freq == 0);

  int id = obj->SFIFO.fifo_id;
  int new_id = id - 1;
  remove_obj_from_list(&params->fifo_heads[id], &params->fifo_tails[id], obj);
  params->fifo_n_bytes[id] -= obj->obj_size;
  params->fifo_n_bytes[new_id] += obj->obj_size;

  obj->SFIFO.fifo_id = new_id;
  obj->SFIFO.freq = 0;
  prepend_obj_to_head(&params->fifo_heads[new_id], &params->fifo_tails[new_id],
                      obj);
  params->fifo_n_objs[id]--;
  params->fifo_n_objs[new_id]++;
}

/**
 * @brief promote the object from the current segment to the next (i+1) segment
 */
static void SFIFO_promote_to_next_seg(cache_t *cache, const request_t *req,
                                      cache_obj_t *obj) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  if (obj->SFIFO.fifo_id == params->n_seg - 1) return;

  int id = obj->SFIFO.fifo_id;
  remove_obj_from_list(&params->fifo_heads[id], &params->fifo_tails[id], obj);
  params->fifo_n_bytes[id] -= obj->obj_size + cache->obj_md_size;
  params->fifo_n_objs[id]--;

  obj->SFIFO.fifo_id += 1;
  obj->SFIFO.freq = 0;
  prepend_obj_to_head(&params->fifo_heads[id + 1], &params->fifo_tails[id + 1],
                      obj);
  params->fifo_n_bytes[id + 1] += obj->obj_size + cache->obj_md_size;
  params->fifo_n_objs[id + 1]++;
}

// ***********************************************************************
// ****                                                               ****
// ****                   debug functions                             ****
// ****                                                               ****
// ***********************************************************************
static void _SFIFO_verify_fifo_size(cache_t *cache) {
  SFIFO_params_t *params = (SFIFO_params_t *)cache->eviction_params;
  for (int i = 0; i < params->n_seg; i++) {
    int64_t n_objs = 0;
    int64_t n_bytes = 0;
    cache_obj_t *obj = params->fifo_heads[i];
    while (obj != NULL) {
      n_objs += 1;
      n_bytes += obj->obj_size;
      obj = obj->queue.next;
    }
    assert(n_objs == params->fifo_n_objs[i]);
    assert(n_bytes == params->fifo_n_bytes[i]);
  }
}

bool SFIFO_get_debug(cache_t *cache, const request_t *req) {
  cache->n_req += 1;

  // SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  cache->last_request_metadata = "none";
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  bool cache_hit = cache->find(cache, req, true) != NULL;
  if (cache_hit) {
    cache->last_request_metadata = "hit";
  } else {
    cache->last_request_metadata = "miss";
  }
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  if (cache_hit) {
    return cache_hit;
  }

  if (cache->can_insert(cache, req) == false) {
    return cache_hit;
  }

  if (!cache_hit) {
    while (cache->occupied_byte + req->obj_size + cache->obj_md_size >
           cache->cache_size) {
      cache->evict(cache, req);
    }

    cache->insert(cache, req);
  }

  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  return cache_hit;
}

#ifdef __cplusplus
extern "C"
}
#endif