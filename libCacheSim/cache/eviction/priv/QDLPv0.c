/**
 * @file QDLP.c
 * @brief Implementation of QDLPv0 eviction algorithm
 * QDLPv0: lazy promotion and quick demotion
 * it uses a probationary FIFO queue with a sieve cache
 * objects are first inserted into the FIFO queue, and moved to the clock cache
 * when evicting from the FIFO queue if it has been accessed
 * if the cache is full, evict from the FIFO cache
 * if the clock cache is full, then promoting to clock will trigger an eviction
 * from the clock cache (not insert into the FIFO queue)
 */

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/cache.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define DEBUG_MODE

// *********************************************************************** //
// ****                        debug macro                            **** //
#ifdef DEBUG_MODE
#define PRINT_CACHE_STATE(cache, params)                                \
  printf("cache size %ld + %ld + %ld, %ld %ld \t", params->n_fifo_byte, \
         params->n_fifo_ghost_byte, params->n_clock_byte,               \
         cache->occupied_size, cache->cache_size);                      \
  // j
#define DEBUG_PRINT(FMT, ...)         \
  do {                                \
    PRINT_CACHE_STATE(cache, params); \
    printf(FMT, ##__VA_ARGS__);       \
    printf("%s", NORMAL);             \
    fflush(stdout);                   \
  } while (0)

#else
#define DEBUG_PRINT(FMT, ...)
#endif
// *********************************************************************** //

typedef struct {
  cache_obj_t *fifo_head;
  cache_obj_t *fifo_tail;

  cache_obj_t *fifo_ghost_head;
  cache_obj_t *fifo_ghost_tail;

  cache_obj_t *clock_head;
  cache_obj_t *clock_tail;
  int64_t n_fifo_obj;
  int64_t n_fifo_byte;
  int64_t n_fifo_ghost_obj;
  int64_t n_fifo_ghost_byte;
  int64_t n_clock_obj;
  int64_t n_clock_byte;
  // the user specified size
  double fifo_ratio;
  int64_t fifo_size;
  int64_t clock_size;
  int64_t fifo_ghost_size;
  cache_obj_t *clock_pointer;
  int64_t vtime_last_check_is_ghost;

  bool lazy_promotion;
} QDLPv0_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void QDLPv0_parse_params(cache_t *cache,
                              const char *cache_specific_params);
static void QDLPv0_free(cache_t *cache);
static bool QDLPv0_get(cache_t *cache, const request_t *req);
static cache_obj_t *QDLPv0_find(cache_t *cache, const request_t *req,
                              const bool update_cache);
static cache_obj_t *QDLPv0_insert(cache_t *cache, const request_t *req);
static cache_obj_t *QDLPv0_to_evict(cache_t *cache, const request_t *req);
static void QDLPv0_evict(cache_t *cache, const request_t *req);
static bool QDLPv0_remove(cache_t *cache, const obj_id_t obj_id);

/* internal functions */
static void QDLPv0_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove);

static void QDLPv0_clock_evict(cache_t *cache, const request_t *req);
static void QDLPv0_parse_params(cache_t *cache,
                              const char *cache_specific_params);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************
/**
 * @brief initialize a QDLPv0 cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params QDLPv0 specific parameters, should be NULL
 */
cache_t *QDLPv0_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("QDLPv0", ccache_params, cache_specific_params);
  cache->cache_init = QDLPv0_init;
  cache->cache_free = QDLPv0_free;
  cache->get = QDLPv0_get;
  cache->find = QDLPv0_find;
  cache->insert = QDLPv0_insert;
  cache->evict = QDLPv0_evict;
  cache->remove = QDLPv0_remove;
  cache->to_evict = QDLPv0_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 1;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc(QDLPv0_params_t);
  QDLPv0_params_t *params = (QDLPv0_params_t *)cache->eviction_params;
  memset(cache->eviction_params, 0, sizeof(QDLPv0_params_t));
  params->clock_pointer = NULL;
  params->fifo_ratio = 0.2;
  params->lazy_promotion = true;

  params->n_fifo_obj = 0;
  params->n_fifo_byte = 0;
  params->n_fifo_ghost_obj = 0;
  params->n_fifo_ghost_byte = 0;
  params->n_clock_obj = 0;
  params->n_clock_byte = 0;
  params->fifo_head = NULL;
  params->fifo_tail = NULL;
  params->clock_head = NULL;
  params->clock_tail = NULL;
  params->fifo_ghost_head = NULL;
  params->fifo_ghost_tail = NULL;
  params->vtime_last_check_is_ghost = -1;

  if (cache_specific_params != NULL) {
    QDLPv0_parse_params(cache, cache_specific_params);
  }

  params->fifo_size = cache->cache_size * params->fifo_ratio;
  params->clock_size = cache->cache_size - params->fifo_size;
  params->fifo_ghost_size = params->clock_size;
  if (params->lazy_promotion) {
    snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "QDLPv0-%.4lf-lazy",
             params->fifo_ratio);
  } else {
    snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "QDLPv0-%.4lf",
             params->fifo_ratio);
  }

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void QDLPv0_free(cache_t *cache) {
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
static bool QDLPv0_get(cache_t *cache, const request_t *req) {
  QDLPv0_params_t *params = cache->eviction_params;
  bool cache_hit = cache_get_base(cache, req);
  DEBUG_PRINT("%ld QDLPv0_get2\n", cache->n_req);
  DEBUG_ASSERT(params->n_fifo_obj + params->n_clock_obj == cache->n_obj);
  DEBUG_ASSERT(params->n_fifo_byte + params->n_clock_byte ==
               cache->occupied_byte);

  return cache_hit;
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
static cache_obj_t *QDLPv0_find(cache_t *cache, const request_t *req,
                              const bool update_cache) {
  QDLPv0_params_t *params = cache->eviction_params;
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);
  cache_obj_t *ret = cache_obj;
  bool cache_hit = (cache_obj != NULL && cache_obj->QDLP.cache_id != 3);
  DEBUG_PRINT("%ld QDLPv0_find %s\n", cache->n_req, cache_hit ? "hit" : "miss");

  if (cache_obj != NULL && update_cache) {
    if (cache_obj->QDLP.cache_id == 1) {
      cache_obj->QDLP.freq += 1;
      if (!params->lazy_promotion) {
        /* FIFO cache, promote to clock cache */
        remove_obj_from_list(&params->fifo_head, &params->fifo_tail, cache_obj);
        params->n_fifo_obj--;
        params->n_fifo_byte -= cache_obj->obj_size + cache->obj_md_size;
        prepend_obj_to_head(&params->clock_head, &params->clock_tail,
                            cache_obj);
        params->n_clock_obj++;
        params->n_clock_byte += cache_obj->obj_size + cache->obj_md_size;
        cache_obj->QDLP.cache_id = 2;
        cache_obj->QDLP.freq = 0;
        while (params->n_clock_byte > params->clock_size) {
          // clock cache is full, evict from clock cache
          QDLPv0_clock_evict(cache, req);
        }
      }
    } else if (cache_obj->QDLP.cache_id == 2) {
      // clock cache
      DEBUG_PRINT("%ld QDLPv0_find hit on clock\n", cache->n_req);
      if (cache_obj->QDLP.freq < 1) {
        // using one-bit, using multi-bit reduce miss ratio most of the time
        cache_obj->QDLP.freq += 1;
      }
    } else if (cache_obj->QDLP.cache_id == 3) {
      // FIFO ghost
      DEBUG_PRINT("%ld QDLPv0_check ghost\n", cache->n_req);
      DEBUG_ASSERT(cache_hit == false);
      params->vtime_last_check_is_ghost = cache->n_req;
      QDLPv0_remove_obj(cache, cache_obj);
    } else {
      ERROR("cache_obj->QDLP.cache_id = %d", cache_obj->QDLP.cache_id);
    }
  }

  if (cache_hit)
    return ret;
  else
    return NULL;
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
static cache_obj_t *QDLPv0_insert(cache_t *cache, const request_t *req) {
  QDLPv0_params_t *params = cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->QDLP.visited = false;

  if (cache->n_req == params->vtime_last_check_is_ghost) {
    // insert to Clock
    DEBUG_PRINT("%ld QDLPv0_insert to clock\n", cache->n_req);
    prepend_obj_to_head(&params->clock_head, &params->clock_tail, obj);
    params->n_clock_obj++;
    params->n_clock_byte += obj->obj_size + cache->obj_md_size;
    obj->QDLP.cache_id = 2;
    obj->QDLP.freq = 0;

  } else {
    // insert to FIFO
    DEBUG_PRINT("%ld QDLPv0_insert to fifo\n", cache->n_req);
    prepend_obj_to_head(&params->fifo_head, &params->fifo_tail, obj);
    params->n_fifo_obj++;
    params->n_fifo_byte += obj->obj_size + cache->obj_md_size;
    obj->QDLP.cache_id = 1;
    obj->QDLP.freq = 0;
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
static cache_obj_t *QDLPv0_to_evict(cache_t *cache, const request_t *req) {
  QDLPv0_params_t *params = cache->eviction_params;
  if (params->fifo_tail != NULL) {
    return params->fifo_tail;
  } else {
    // not implemented, we need to evict from the clock cache
    assert(0);
  }
  return NULL;
}

/**
 * @brief evict an object from the clock
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 * @param evicted_obj if not NULL, return the evicted object to caller
 */
static void QDLPv0_clock_evict(cache_t *cache, const request_t *reqj) {
  QDLPv0_params_t *params = cache->eviction_params;
  cache_obj_t *pointer = params->clock_pointer;

  /* if we have run one full around or first eviction */
  if (pointer == NULL) {
    pointer = params->clock_tail;
  }

  /* find the first untouched */
  int n_loop = 0;
  while (pointer->QDLP.freq > 0) {
    pointer->QDLP.freq -= 1;
    pointer = pointer->queue.prev;
    if (pointer == NULL) {
      n_loop += 1;
      pointer = params->clock_tail;
      assert(n_loop < 1000);
    }
  }

  params->clock_pointer = pointer->queue.prev;
  remove_obj_from_list(&params->clock_head, &params->clock_tail, pointer);
  params->n_clock_obj--;
  params->n_clock_byte -= pointer->obj_size + cache->obj_md_size;
  cache_evict_base(cache, pointer, true);
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
static void QDLPv0_evict(cache_t *cache, const request_t *req) {
  QDLPv0_params_t *params = cache->eviction_params;

  if (params->n_fifo_byte > params->fifo_size || params->n_clock_obj == 0) {
    DEBUG_PRINT("%ld QDLPv0_evict_fifo\n", cache->n_req);
    cache_obj_t *obj_to_evict = params->fifo_tail;
    DEBUG_ASSERT(obj_to_evict != NULL);

    // remove from FIFO
    remove_obj_from_list(&params->fifo_head, &params->fifo_tail, obj_to_evict);
    params->n_fifo_obj--;
    params->n_fifo_byte -= obj_to_evict->obj_size + cache->obj_md_size;

    if (params->lazy_promotion && obj_to_evict->QDLP.freq > 0) {
      prepend_obj_to_head(&params->clock_head, &params->clock_tail,
                          obj_to_evict);
      params->n_clock_obj++;
      params->n_clock_byte += obj_to_evict->obj_size + cache->obj_md_size;
      obj_to_evict->QDLP.cache_id = 2;
      obj_to_evict->QDLP.freq = 0;
      while (params->n_clock_byte > params->clock_size) {
        // clock cache is full, evict from clock cache
        QDLPv0_clock_evict(cache, req);
      }
    } else {
      cache_evict_base(cache, obj_to_evict, false);
      prepend_obj_to_head(&params->fifo_ghost_head, &params->fifo_ghost_tail,
                          obj_to_evict);
      params->n_fifo_ghost_obj++;
      params->n_fifo_ghost_byte += obj_to_evict->obj_size + cache->obj_md_size;
      obj_to_evict->QDLP.cache_id = 3;
      while (params->n_fifo_ghost_byte > params->fifo_ghost_size) {
        // clock cache is full, evict from clock cache
        QDLPv0_remove_obj(cache, params->fifo_ghost_tail);
      }
    }
  } else {
    DEBUG_PRINT("%ld QDLPv0_evict_clock\n", cache->n_req);
    // evict from clock cache, this can happen because we insert to the clock
    // when the object hit on the ghost
    QDLPv0_clock_evict(cache, req);
  }
}

static void QDLPv0_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  DEBUG_ASSERT(obj_to_remove != NULL);
  QDLPv0_params_t *params = cache->eviction_params;

  if (obj_to_remove->QDLP.cache_id == 1) {
    // fifo cache
    remove_obj_from_list(&params->fifo_head, &params->fifo_tail, obj_to_remove);
    params->n_fifo_obj--;
    params->n_fifo_byte -= obj_to_remove->obj_size + cache->obj_md_size;
    cache_remove_obj_base(cache, obj_to_remove, true);
  } else if (obj_to_remove->QDLP.cache_id == 2) {
    // clock cache
    remove_obj_from_list(&params->clock_head, &params->clock_tail,
                         obj_to_remove);
    params->n_clock_obj--;
    params->n_clock_byte -= obj_to_remove->obj_size + cache->obj_md_size;
    cache_remove_obj_base(cache, obj_to_remove, true);
  } else if (obj_to_remove->QDLP.cache_id == 3) {
    // fifo ghost
    remove_obj_from_list(&params->fifo_ghost_head, &params->fifo_ghost_tail,
                         obj_to_remove);
    params->n_fifo_ghost_obj--;
    params->n_fifo_ghost_byte -= obj_to_remove->obj_size + cache->obj_md_size;
    hashtable_delete(cache->hashtable, obj_to_remove);
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
static bool QDLPv0_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }
  QDLPv0_remove_obj(cache, obj);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *QDLPv0_current_params(QDLPv0_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "fifo-ratio=%.4lf\n", params->fifo_ratio);
  return params_str;
}

static void QDLPv0_parse_params(cache_t *cache,
                              const char *cache_specific_params) {
  QDLPv0_params_t *params = (QDLPv0_params_t *)cache->eviction_params;
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

    if (strcasecmp(key, "fifo-ratio") == 0) {
      params->fifo_ratio = (double)strtof(value, &end);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }

    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", QDLPv0_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }
  free(old_params_str);
}

#ifdef __cplusplus
}
#endif
