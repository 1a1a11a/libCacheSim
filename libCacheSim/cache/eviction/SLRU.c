//
//  segmented LRU implemented using multiple lists instead of multiple LRUs
//  this has a better performance than SLRUv0, but it is very hard to implement
//
//  SLRU.c
//  libCacheSim
//
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define USE_BELADY
#define SLRU_MAX_N_SEG 16
#define DEBUG_MODE
#undef DEBUG_MODE

typedef struct SLRU_params {
  cache_obj_t **lru_heads;
  cache_obj_t **lru_tails;
  int64_t *lru_n_bytes;
  int64_t *lru_n_objs;
  int64_t *lru_max_n_bytes;
  int n_seg;
} SLRU_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void SLRU_free(cache_t *cache);
static bool SLRU_get(cache_t *cache, const request_t *req);
static cache_obj_t *SLRU_find(cache_t *cache, const request_t *req,
                              const bool update_cache);
static cache_obj_t *SLRU_insert(cache_t *cache, const request_t *req);
static cache_obj_t *SLRU_to_evict(cache_t *cache, const request_t *req);
static void SLRU_evict(cache_t *cache, const request_t *req);
static bool SLRU_remove(cache_t *cache, const obj_id_t obj_id);

/* internal function */
static void SLRU_promote_to_next_seg(cache_t *cache, const request_t *req,
                                     cache_obj_t *obj);
static void SLRU_cool(cache_t *cache, const request_t *req, const int id);
bool SLRU_can_insert(cache_t *cache, const request_t *req);
static void SLRU_parse_params(cache_t *cache,
                              const char *cache_specific_params);

// ***********************************************************************
// ****                                                               ****
// ****                       debug functions                         ****
// ****                                                               ****
// ***********************************************************************
#ifdef DEBUG_MODE
#define DEBUG_PRINT_CACHE_STATE(cache, params, req)                            \
  do {                                                                         \
    printf("%ld %ld %s: ", cache->n_req, req->obj_id, __func__);               \
    for (int i = 0; i < params->n_seg; i++) {                                  \
      printf("%ld/%ld/%p/%p, ", params->lru_n_objs[i], params->lru_n_bytes[i], \
             params->lru_heads[i], params->lru_tails[i]);                      \
    }                                                                          \
    printf("\n");                                                              \
    _SLRU_verify_lru_size(cache);                                              \
  } while (0)

#define DEBUG_PRINT_CACHE(cache, params)                 \
  do {                                                   \
    for (int i = params->n_seg - 1; i >= 0; i--) {       \
      cache_obj_t *obj = params->lru_heads[i];           \
      while (obj != NULL) {                              \
        printf("%lu(%u)->", obj->obj_id, obj->obj_size); \
        obj = obj->queue.next;                           \
      }                                                  \
      printf(" | ");                                     \
    }                                                    \
    printf("\n");                                        \
  } while (0)

static void _SLRU_verify_lru_size(cache_t *cache);
bool SLRU_get_debug(cache_t *cache, const request_t *req);

#else
#define DEBUG_PRINT_CACHE_STATE(cache, params, req)
#define DEBUG_PRINT_CACHE(cache, params)
#endif

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************
/**
 * @brief initialize a LRU cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params LRU specific parameters, should be NULL
 */
cache_t *SLRU_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("SLRU", ccache_params, cache_specific_params);
  cache->cache_init = SLRU_init;
  cache->cache_free = SLRU_free;
  cache->get = SLRU_get;
  cache->find = SLRU_find;
  cache->insert = SLRU_insert;
  cache->evict = SLRU_evict;
  cache->remove = SLRU_remove;
  cache->to_evict = SLRU_to_evict;
  cache->can_insert = SLRU_can_insert;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = (SLRU_params_t *)malloc(sizeof(SLRU_params_t));
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  memset(params, 0, sizeof(SLRU_params_t));
  params->n_seg = 4;

  if (cache_specific_params != NULL) {
    SLRU_parse_params(cache, cache_specific_params);
  }

  if (params->lru_max_n_bytes == NULL) {
    // if the user does not specify segment size
    params->lru_max_n_bytes = calloc(params->n_seg, sizeof(int64_t));
    for (int i = 0; i < params->n_seg; i++) {
      params->lru_max_n_bytes[i] =
          (int64_t)ccache_params.cache_size / params->n_seg;
    }
  }

  params->lru_heads =
      (cache_obj_t **)malloc(sizeof(cache_obj_t *) * params->n_seg);
  params->lru_tails =
      (cache_obj_t **)malloc(sizeof(cache_obj_t *) * params->n_seg);
  params->lru_n_objs = (int64_t *)malloc(sizeof(int64_t) * params->n_seg);
  params->lru_n_bytes = (int64_t *)malloc(sizeof(int64_t) * params->n_seg);

  for (int i = 0; i < params->n_seg; i++) {
    params->lru_heads[i] = NULL;
    params->lru_tails[i] = NULL;
    params->lru_n_objs[i] = 0;
    params->lru_n_bytes[i] = 0;
  }

  // update slru cache name
  bool same_size = true;
  for (int i = 1; i < params->n_seg; i++) {
    if (params->lru_max_n_bytes[i] != params->lru_max_n_bytes[i - 1]) {
      same_size = false;
      break;
    }
  }
  int n = 0;
  if (same_size) {
    n = snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "S%dLRU(%d",
                 params->n_seg, (int)(100 / params->n_seg));
    for (int i = 1; i < params->n_seg; i++) {
      n += snprintf(cache->cache_name + n, CACHE_NAME_ARRAY_LEN - n, ":%d",
                    (int)(100 / params->n_seg));
    }
  } else {
    n = snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "S%dLRU(%d",
                 params->n_seg,
                 (int)(params->lru_max_n_bytes[0] * 100 / cache->cache_size));

    for (int i = 1; i < params->n_seg; i++) {
      n +=
          snprintf(cache->cache_name + n, CACHE_NAME_ARRAY_LEN - n, ":%d",
                   (int)(params->lru_max_n_bytes[i] * 100 / cache->cache_size));
    }
  }
  snprintf(cache->cache_name + n, CACHE_NAME_ARRAY_LEN - n, ")");

#ifdef USE_BELADY
  char *tmp = strdup(cache->cache_name);
  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "%s_Belady", tmp);
  free(tmp);
#endif

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void SLRU_free(cache_t *cache) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  free(params->lru_max_n_bytes);
  free(params->lru_heads);
  free(params->lru_tails);
  free(params->lru_n_objs);
  free(params->lru_n_bytes);
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
static bool SLRU_get(cache_t *cache, const request_t *req) {
#ifdef DEBUG_MODE
  return SLRU_get_debug(cache, req);
#else
  bool ck = cache_get_base(cache, req);
#endif
  return ck;
}

/**
 * @brief check whether an object is in the cache,
 * promote to the next segment if update_cache is true
 */
static cache_obj_t *SLRU_find(cache_t *cache, const request_t *req,
                              const bool update_cache) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  cache_obj_t *obj = hashtable_find(cache->hashtable, req);

  if (obj == NULL || !update_cache) {
    return obj;
  }

#ifdef USE_BELADY
  obj->next_access_vtime = req->next_access_vtime;
  if (obj->next_access_vtime == INT64_MAX) {
    return obj;
  }
#endif

  if (obj->SLRU.lru_id == params->n_seg - 1) {
    move_obj_to_head(&params->lru_heads[params->n_seg - 1],
                     &params->lru_tails[params->n_seg - 1], obj);
  } else {
    SLRU_promote_to_next_seg(cache, req, obj);

    while (params->lru_n_bytes[obj->SLRU.lru_id] >
           params->lru_max_n_bytes[obj->SLRU.lru_id]) {
      // if the LRU is full
      SLRU_cool(cache, req, obj->SLRU.lru_id);
    }
    DEBUG_ASSERT(cache->occupied_byte <= cache->cache_size);
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
static cache_obj_t *SLRU_insert(cache_t *cache, const request_t *req) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  // Find the lowest LRU with space for insertion
  int nth_seg = -1;
  for (int i = 0; i < params->n_seg; i++) {
    if (params->lru_n_bytes[i] + req->obj_size + cache->obj_md_size <=
        params->lru_max_n_bytes[i]) {
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

  cache_obj_t *obj = cache_insert_base(cache, req);

#ifdef USE_BELADY
  obj->next_access_vtime = req->next_access_vtime;
#endif

  prepend_obj_to_head(&params->lru_heads[nth_seg], &params->lru_tails[nth_seg],
                      obj);
  obj->SLRU.lru_id = nth_seg;
  params->lru_n_bytes[nth_seg] += req->obj_size + cache->obj_md_size;
  params->lru_n_objs[nth_seg]++;

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
static cache_obj_t *SLRU_to_evict(cache_t *cache, const request_t *req) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);
  for (int i = 0; i < params->n_seg; i++) {
    if (params->lru_n_bytes[i] > 0) {
      return params->lru_tails[i];
    }
  }
// No object to evict
#ifdef DEBUG_MODE
  printf("No object to evict, please check whether this is unexpected\n");
#endif
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
static void SLRU_evict(cache_t *cache, const request_t *req) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);

  cache_obj_t *obj = SLRU_to_evict(cache, req);

  params->lru_n_bytes[obj->SLRU.lru_id] -= obj->obj_size + cache->obj_md_size;
  params->lru_n_objs[obj->SLRU.lru_id]--;

  remove_obj_from_list(&params->lru_heads[obj->SLRU.lru_id],
                       &params->lru_tails[obj->SLRU.lru_id], obj);
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
static bool SLRU_remove(cache_t *cache, const obj_id_t obj_id) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);

  if (obj == NULL) {
    return false;
  }

  remove_obj_from_list(&(params->lru_heads[obj->SLRU.lru_id]),
                       &(params->lru_tails[obj->SLRU.lru_id]), obj);
  cache_remove_obj_base(cache, obj, true);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                  parameter set up functions                   ****
// ****                                                               ****
// ***********************************************************************
static const char *SLRU_current_params(cache_t *cache, SLRU_params_t *params) {
  static __thread char params_str[128];
  int n = snprintf(params_str, 128, "n-seg=%d,seg-size=%d", params->n_seg,
                   (int)(params->lru_max_n_bytes[0] * 100 / cache->cache_size));

  for (int i = 1; i < params->n_seg; i++) {
    n += snprintf(params_str + n, 128 - n, ":%d",
                  (int)(params->lru_max_n_bytes[i] * 100 / cache->cache_size));
  }

  return params_str;
}

static void SLRU_parse_params(cache_t *cache,
                              const char *cache_specific_params) {
  SLRU_params_t *params = (SLRU_params_t *)cache->eviction_params;
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
    } else if (strcasecmp(key, "seg-size") == 0) {
      int n_seg = 0;
      int64_t seg_size_sum = 0;
      int64_t seg_size_array[SLRU_MAX_N_SEG];
      char *v = strsep((char **)&value, ":");
      while (v != NULL) {
        seg_size_array[n_seg++] = (int64_t)strtol(v, &end, 0);
        seg_size_sum += seg_size_array[n_seg - 1];
        v = strsep((char **)&value, ":");
      }
      params->n_seg = n_seg;
      params->lru_max_n_bytes = calloc(params->n_seg, sizeof(int64_t));
      for (int i = 0; i < n_seg; i++) {
        params->lru_max_n_bytes[i] = (int64_t)(
            (double)seg_size_array[i] / seg_size_sum * cache->cache_size);
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", SLRU_current_params(cache, params));
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
// ****                       internal functions                      ****
// ****                                                               ****
// ***********************************************************************
/* SLRU cannot insert an object larger than segment size */
bool SLRU_can_insert(cache_t *cache, const request_t *req) {
  SLRU_params_t *params = (SLRU_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);
  return can_insert &&
         (req->obj_size + cache->obj_md_size <= params->lru_max_n_bytes[0]);
}

/**
 * @brief move an object from ith LRU into (i-1)th LRU, cool
 * (i-1)th LRU if it is full, where the n_seg th LRU is the most recent
 *
 * @param cache
 * @param i
 */
static void SLRU_cool(cache_t *cache, const request_t *req, const int id) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  if (id == 0) return SLRU_evict(cache, req);

  cache_obj_t *obj = params->lru_tails[id];
  DEBUG_ASSERT(obj != NULL);
  DEBUG_ASSERT(obj->SLRU.lru_id == id);
  remove_obj_from_list(&params->lru_heads[id], &params->lru_tails[id], obj);
  prepend_obj_to_head(&params->lru_heads[id - 1], &params->lru_tails[id - 1],
                      obj);
  obj->SLRU.lru_id = id - 1;
  params->lru_n_bytes[id] -= obj->obj_size;
  params->lru_n_bytes[id - 1] += obj->obj_size;
  params->lru_n_objs[id]--;
  params->lru_n_objs[id - 1]++;

  // If lower LRUs are full
  while (params->lru_n_bytes[id - 1] > params->lru_max_n_bytes[id - 1]) {
    SLRU_cool(cache, req, id - 1);
  }
}

/**
 * @brief promote the object from the current segment to the next (i+1)
 * segment
 */
static void SLRU_promote_to_next_seg(cache_t *cache, const request_t *req,
                                     cache_obj_t *obj) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  int id = obj->SLRU.lru_id;
  remove_obj_from_list(&params->lru_heads[id], &params->lru_tails[id], obj);
  params->lru_n_bytes[id] -= obj->obj_size + cache->obj_md_size;
  params->lru_n_objs[id]--;

  obj->SLRU.lru_id += 1;
  prepend_obj_to_head(&params->lru_heads[id + 1], &params->lru_tails[id + 1],
                      obj);
  params->lru_n_bytes[id + 1] += obj->obj_size + cache->obj_md_size;
  params->lru_n_objs[id + 1]++;
}

// ############################## debug functions ##############################
static void _SLRU_verify_lru_size(cache_t *cache) {
  SLRU_params_t *params = (SLRU_params_t *)cache->eviction_params;
  for (int i = 0; i < params->n_seg; i++) {
    int64_t n_objs = 0;
    int64_t n_bytes = 0;
    cache_obj_t *obj = params->lru_heads[i];
    while (obj != NULL) {
      n_objs += 1;
      n_bytes += obj->obj_size;
      obj = obj->queue.next;
    }
    assert(n_objs == params->lru_n_objs[i]);
    assert(n_bytes == params->lru_n_bytes[i]);
  }
}

bool SLRU_get_debug(cache_t *cache, const request_t *req) {
  // SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  cache->n_req += 1;

  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  bool cache_hit = cache->find(cache, req, true) != NULL;

  if (cache_hit) {
    DEBUG_PRINT_CACHE(cache, params);
    return cache_hit;
  }

  if (cache->can_insert(cache, req) == false) {
    DEBUG_PRINT_CACHE(cache, params);
    return cache_hit;
  }

  if (!cache_hit) {
    while (cache->occupied_byte + req->obj_size + cache->obj_md_size >
           cache->cache_size) {
      cache->evict(cache, req);
    }

    cache->insert(cache, req);
  }

  DEBUG_PRINT_CACHE(cache, params);

  return cache_hit;
}

#ifdef __cplusplus
extern "C"
}
#endif