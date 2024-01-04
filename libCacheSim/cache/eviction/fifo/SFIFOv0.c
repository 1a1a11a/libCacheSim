//
//  segment FIFO
//
//  cache hit promotes an object to the next FIFO
//      no promotion if it is in the last FIFO
//  cache miss inserts an object into the first FIFO
//  if not use Belady,
//      evict from the first FIFO
//  else
//      evict from one of the FIFOs based on distance to the next request
//
//
//  we notice that adding belady is not helpful
//
//
//  libCacheSim
//
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SFIFOv0_params {
  cache_t **FIFOs;
  int n_queues;
  // a temporary request used to move object between FIFOs
  request_t *req_local;
} SFIFOv0_params_t;

// #define USE_BELADY
static const char *DEFAULT_CACHE_PARAMS = "n-queue=4";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void SFIFOv0_parse_params(cache_t *cache,
                                 const char *cache_specific_params);
static void SFIFOv0_free(cache_t *cache);
static bool SFIFOv0_get(cache_t *cache, const request_t *req);
static cache_obj_t *SFIFOv0_find(cache_t *cache, const request_t *req,
                                 const bool update_cache);
static cache_obj_t *SFIFOv0_insert(cache_t *cache, const request_t *req);
static cache_obj_t *SFIFOv0_to_evict(cache_t *cache, const request_t *req);
static void SFIFOv0_evict(cache_t *cache, const request_t *req);
static bool SFIFOv0_remove(cache_t *cache, const obj_id_t obj_id);
static void SFIFOv0_cool(cache_t *cache, const request_t *req, int i);

/* SFIFOv0 cannot an object larger than segment size */
static inline bool SFIFOv0_can_insert(cache_t *cache, const request_t *req);
static inline int64_t SFIFOv0_get_occupied_byte(const cache_t *cache);
static inline int64_t SFIFOv0_get_n_obj(const cache_t *cache);

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
cache_t *SFIFOv0_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("SFIFOv0", ccache_params, cache_specific_params);
  cache->cache_init = SFIFOv0_init;
  cache->cache_free = SFIFOv0_free;
  cache->get = SFIFOv0_get;
  cache->find = SFIFOv0_find;
  cache->insert = SFIFOv0_insert;
  cache->evict = SFIFOv0_evict;
  cache->remove = SFIFOv0_remove;
  cache->to_evict = SFIFOv0_to_evict;
  cache->can_insert = SFIFOv0_can_insert;
  cache->get_occupied_byte = SFIFOv0_get_occupied_byte;
  cache->get_n_obj = SFIFOv0_get_n_obj;

  cache->obj_md_size = 0;

  cache->eviction_params = (SFIFOv0_params_t *)malloc(sizeof(SFIFOv0_params_t));
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)(cache->eviction_params);

  if (cache_specific_params != NULL) {
    SFIFOv0_parse_params(cache, cache_specific_params);
  } else {
    SFIFOv0_parse_params(cache, DEFAULT_CACHE_PARAMS);
  }

  params->FIFOs = (cache_t **)malloc(sizeof(cache_t *) * params->n_queues);

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size /= params->n_queues;
  ccache_params_local.hashpower /= MIN(16, ccache_params_local.hashpower - 4);
  for (int i = 0; i < params->n_queues; i++) {
    params->FIFOs[i] = FIFO_init(ccache_params_local, NULL);
  }
  params->req_local = new_request();

#ifdef USE_BELADY
  snprintf(cache->cache_name, 20, "SFIFOv0-%d-belady", params->n_queues);
#else
  snprintf(cache->cache_name, 20, "SFIFOv0-%d", params->n_queues);
#endif

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void SFIFOv0_free(cache_t *cache) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)(cache->eviction_params);
  free_request(params->req_local);
  for (int i = 0; i < params->n_queues; i++)
    params->FIFOs[i]->cache_free(params->FIFOs[i]);
  free(params->FIFOs);
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
static bool SFIFOv0_get(cache_t *cache, const request_t *req) {
  bool found = cache_get_base(cache, req);

  return found;
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
static cache_obj_t *SFIFOv0_find(cache_t *cache, const request_t *req,
                                 const bool update_cache) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)(cache->eviction_params);

  cache_obj_t *obj = NULL;
  for (int i = 0; i < params->n_queues; i++) {
    obj = params->FIFOs[i]->find(params->FIFOs[i], req, false);
    bool cache_hit = obj != NULL;

    if (cache_hit) {
      // bump object from lower segment to upper segment;
      if (i != params->n_queues - 1) {
        params->FIFOs[i]->remove(params->FIFOs[i], req->obj_id);

        // If the upper FIFO is full
        cache_t *next_fifo = params->FIFOs[i + 1];
        while (next_fifo->get_occupied_byte(next_fifo) + req->obj_size +
                   cache->obj_md_size >
               next_fifo->cache_size)
          SFIFOv0_cool(cache, req, i + 1);

        cache_obj_t *new_obj = next_fifo->insert(next_fifo, req);
        new_obj->misc.next_access_vtime = req->next_access_vtime;
      } else {
        obj->misc.next_access_vtime = req->next_access_vtime;
      }

      return obj;
    }
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
static cache_obj_t *SFIFOv0_insert(cache_t *cache, const request_t *req) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)(cache->eviction_params);

  // Find the lowest FIFO with space for insertion
  for (int i = 0; i < params->n_queues; i++) {
    cache_t *fifo = params->FIFOs[i];
    if (fifo->get_occupied_byte(fifo) + req->obj_size + cache->obj_md_size <=
        fifo->cache_size) {
      cache_obj_t *obj = fifo->insert(fifo, req);
      obj->misc.next_access_vtime = req->next_access_vtime;
      return obj;
    }
  }

  // If all FIFOs are filled, insert into the lowest FIFO.
  cache_t *fifo = params->FIFOs[0];
  cache_obj_t *obj = fifo->insert(fifo, req);
  obj->misc.next_access_vtime = req->next_access_vtime;
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
static cache_obj_t *SFIFOv0_to_evict(cache_t *cache, const request_t *req) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)(cache->eviction_params);
#ifdef USE_BELADY
  cache_obj_t *best_obj = NULL;
  double best_obj_score = 0;
  int best_obj_fifo_idx = 0;
  for (int i = 0; i < params->n_queues; i++) {
    cache_t *fifo = params->FIFOs[i];
    cache_obj_t *obj = fifo->to_evict(fifo, req);
    if (obj == NULL) continue;
    if (best_obj == NULL || obj->next_access_vtime > best_obj_score) {
      best_obj = obj;
      best_obj_score = obj->next_access_vtime;
      best_obj_fifo_idx = i;
    }

    return best_obj;
  }
#else

  for (int i = 0; i < params->n_queues; i++) {
    cache_t *fifo = params->FIFOs[i];
    if (fifo->get_occupied_byte(fifo) > 0) {
      return fifo->to_evict(fifo, req);
    }
  }
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
static void SFIFOv0_evict(cache_t *cache, const request_t *req) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)(cache->eviction_params);
#ifdef USE_BELADY
  cache_obj_t *best_obj = NULL;
  double best_obj_score = 0;
  int best_obj_fifo_idx = 0;
  for (int i = 0; i < params->n_queues; i++) {
    cache_t *fifo = params->FIFOs[i];
    cache_obj_t *obj = fifo->to_evict(fifo, req);
    if (obj == NULL) continue;
    if (best_obj == NULL || obj->next_access_vtime > best_obj_score) {
      best_obj = obj;
      best_obj_score = obj->next_access_vtime;
      best_obj_fifo_idx = i;
    }
  }
  cache_t *fifo = params->FIFOs[best_obj_fifo_idx];
  fifo->evict(fifo, req);
#else

  for (int i = 0; i < params->n_queues; i++) {
    cache_t *fifo = params->FIFOs[i];
    if (fifo->get_occupied_byte(fifo) > 0) {
      fifo->evict(fifo, req);
      return;
    }
  }
#endif
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
static bool SFIFOv0_remove(cache_t *cache, const obj_id_t obj_id) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)(cache->eviction_params);
  for (int i = 0; i < params->n_queues; i++) {
    cache_t *fifo = params->FIFOs[i];
    if (fifo->remove(fifo, obj_id)) {
      return true;
    }
  }
  return false;
}

/* SFIFOv0 cannot an object larger than segment size */
static inline bool SFIFOv0_can_insert(cache_t *cache, const request_t *req) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);
  return can_insert &&
         (req->obj_size + cache->obj_md_size <= params->FIFOs[0]->cache_size);
}

static inline int64_t SFIFOv0_get_occupied_byte(const cache_t *cache) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)cache->eviction_params;
  int64_t occupied_byte = 0;
  for (int i = 0; i < params->n_queues; i++) {
    occupied_byte += params->FIFOs[i]->occupied_byte;
  }
  return occupied_byte;
}

static inline int64_t SFIFOv0_get_n_obj(const cache_t *cache) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)cache->eviction_params;
  int64_t n_obj = 0;
  for (int i = 0; i < params->n_queues; i++) {
    n_obj += params->FIFOs[i]->n_obj;
  }
  return n_obj;
}

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *SFIFOv0_current_params(SFIFOv0_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-queue=%d\n", params->n_queues);
  return params_str;
}

static void SFIFOv0_parse_params(cache_t *cache,
                                 const char *cache_specific_params) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)cache->eviction_params;
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

    if (strcasecmp(key, "n-queue") == 0) {
      params->n_queues = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }

    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", SFIFOv0_current_params(params));
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
 * @brief move an object from ith FIFO into (i-1)th FIFO, cool
 * (i-1)th FIFO if it is full
 *
 * @param cache
 * @param i
 */
static void SFIFOv0_cool(cache_t *cache, const request_t *req, int i) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)(cache->eviction_params);

  cache_t *fifo = params->FIFOs[i];
  // the last FIFO is evict-only, do not move to a lower fifo
  if (i == 0) {
    fifo->evict(fifo, NULL);
    return;
  };

  // the evicted object move to lower fifo
  cache_obj_t *obj_evicted = fifo->to_evict(fifo, req);
  copy_cache_obj_to_request(params->req_local, obj_evicted);
  fifo->evict(fifo, NULL);

  // If lower FIFOs are full
  cache_t *next_fifo = params->FIFOs[i - 1];
  int64_t required_size =
      next_fifo->cache_size - params->req_local->obj_size - cache->obj_md_size;
  while (next_fifo->get_occupied_byte(next_fifo) > required_size) {
    SFIFOv0_cool(cache, req, i - 1);
  }

  next_fifo->insert(next_fifo, params->req_local);
}

// ***********************************************************************
// ****                                                               ****
// ****                       debug functions                         ****
// ****                                                               ****
// ***********************************************************************
static void SFIFOv0_print_cache(cache_t *cache) {
  SFIFOv0_params_t *params = (SFIFOv0_params_t *)cache->eviction_params;
  for (int i = params->n_queues - 1; i >= 0; i--) {
    cache_obj_t *obj =
        ((FIFO_params_t *)params->FIFOs[i]->eviction_params)->q_head;
    while (obj) {
      printf("%ld(%u)->", (long)obj->obj_id, (unsigned int)obj->obj_size);
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
