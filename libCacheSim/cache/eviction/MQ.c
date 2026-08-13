//
//  MQ.c
//  libCacheSim
//
//  MultiQueue (MQ) replacement algorithm
//
//  Yuanyuan Zhou, James Philbin, Kai Li.
//  "The Multi-Queue Replacement Algorithm for Second Level Buffer Caches"
//  USENIX Annual Technical Conference, 2001
//  https://www.usenix.org/legacy/publications/library/proceedings/usenix01/full_papers/zhou/zhou.pdf
//
//  MQ maintains m LRU-ordered queues Q[0]..Q[m-1], each with its head at the
//  MRU end and its tail at the LRU end. An object with access frequency f
//  resides in queue QueueNum(f) = min(log2(f), m-1), so frequently accessed
//  objects live in higher queues and are less likely to be evicted.
//  Each object has an expireTime; when the object at the tail (LRU end) of a
//  queue expires, it is demoted one queue down (the Adjust routine), which
//  captures temporal locality: objects not accessed for a long time gradually
//  descend and are eventually evicted.
//  Evicted objects' ids and frequencies are remembered in a FIFO history
//  buffer Qout; if an object is re-admitted while still in Qout, its previous
//  frequency is restored.
//

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MQ_MAX_N_QUEUE 64

typedef struct MQ_params {
  cache_obj_t **queue_heads;  // head is the MRU end
  cache_obj_t **queue_tails;  // tail is the LRU end
  // per-queue occupancy; not used for eviction decisions, maintained for
  // debug assertions and future stats
  int64_t *queue_n_bytes;
  int64_t *queue_n_objs;
  int n_queues;

  // lifetime (in number of requests) an object can stay in its queue
  // without being accessed before being demoted to the lower queue
  int64_t lifetime;

  // history buffer remembering the ids and frequencies of evicted objects
  cache_t *Qout;
  double Qout_size_ratio;

  // set by MQ_find when the requested object is found in Qout,
  // consumed by MQ_insert. ghost_obj_id keys the state to a specific
  // request: prefetchers (e.g. Mithril) call cache->insert() without a
  // preceding find(), so MQ_insert must not apply a remembered frequency
  // unless it belongs to the object actually being inserted.
  bool hit_on_ghost;
  int32_t ghost_freq;
  obj_id_t ghost_obj_id;

  // virtual time, incremented by Adjust, which runs once per hit and once
  // per insert (approximately once per request; a miss whose insert is
  // skipped does not tick)
  int64_t vtime;

  request_t *req_local;
} MQ_params_t;

static const char *DEFAULT_CACHE_PARAMS =
    "n-queue=8,lifetime=10000,Qout-size-ratio=4.00";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void MQ_free(cache_t *cache);
static bool MQ_get(cache_t *cache, const request_t *req);
static cache_obj_t *MQ_find(cache_t *cache, const request_t *req,
                            bool update_cache);
static cache_obj_t *MQ_insert(cache_t *cache, const request_t *req);
static cache_obj_t *MQ_to_evict(cache_t *cache, const request_t *req);
static void MQ_evict(cache_t *cache, const request_t *req);
static bool MQ_remove(cache_t *cache, obj_id_t obj_id);
static void MQ_parse_params(cache_t *cache, const char *cache_specific_params);

/* internal functions */
static inline int MQ_queue_num(const MQ_params_t *params, int32_t freq);
static void MQ_adjust(cache_t *cache);
static inline void MQ_queue_link(cache_t *cache, cache_obj_t *obj, int queue);
static inline void MQ_queue_unlink(cache_t *cache, cache_obj_t *obj);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief initialize an MQ cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params MQ specific parameters as a string,
 * e.g. "n-queue=8,lifetime=10000,Qout-size-ratio=4.00",
 * use "print" to get the default parameters
 */
cache_t *MQ_init(const common_cache_params_t ccache_params,
                 const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("MQ", ccache_params, cache_specific_params);
  cache->cache_init = MQ_init;
  cache->cache_free = MQ_free;
  cache->get = MQ_get;
  cache->find = MQ_find;
  cache->insert = MQ_insert;
  cache->evict = MQ_evict;
  cache->remove = MQ_remove;
  cache->to_evict = MQ_to_evict;

  if (ccache_params.consider_obj_metadata) {
    // freq (4) + expire_time (8) + queue_id (2)
    cache->obj_md_size = 14;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = malloc(sizeof(MQ_params_t));
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  memset(params, 0, sizeof(MQ_params_t));

  MQ_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    MQ_parse_params(cache, cache_specific_params);
  }

  params->queue_heads =
      (cache_obj_t **)malloc(sizeof(cache_obj_t *) * params->n_queues);
  params->queue_tails =
      (cache_obj_t **)malloc(sizeof(cache_obj_t *) * params->n_queues);
  params->queue_n_bytes = (int64_t *)malloc(sizeof(int64_t) * params->n_queues);
  params->queue_n_objs = (int64_t *)malloc(sizeof(int64_t) * params->n_queues);
  for (int i = 0; i < params->n_queues; i++) {
    params->queue_heads[i] = NULL;
    params->queue_tails[i] = NULL;
    params->queue_n_bytes[i] = 0;
    params->queue_n_objs[i] = 0;
  }

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size =
      (uint64_t)((double)ccache_params.cache_size * params->Qout_size_ratio);
  ccache_params_local.hashpower = ccache_params.hashpower;
  params->Qout = FIFO_init(ccache_params_local, NULL);

  params->hit_on_ghost = false;
  params->ghost_freq = 0;
  params->ghost_obj_id = 0;
  params->vtime = 0;
  params->req_local = new_request();

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "MQ-%d", params->n_queues);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void MQ_free(cache_t *cache) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  params->Qout->cache_free(params->Qout);
  free_request(params->req_local);
  free(params->queue_heads);
  free(params->queue_tails);
  free(params->queue_n_bytes);
  free(params->queue_n_objs);
  free(params);
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
static bool MQ_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

/**
 * @brief check whether an object is in the cache
 * if update_cache is true and the object is found, its frequency is
 * incremented and it is moved to the MRU end of queue QueueNum(freq);
 * if the object is not in the cache, the history buffer Qout is checked
 * and the remembered frequency is restored upon insertion
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return the object or NULL if not found
 */
static cache_obj_t *MQ_find(cache_t *cache, const request_t *req,
                            bool update_cache) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;

  cache_obj_t *obj = hashtable_find(cache->hashtable, req);

  if (!update_cache) {
    return obj;
  }

  if (obj != NULL) {
    /* cache hit: promote to queue QueueNum(freq + 1) */
    MQ_queue_unlink(cache, obj);
    if (obj->freq < INT32_MAX) {
      obj->freq++;
    }
    MQ_queue_link(cache, obj, MQ_queue_num(params, obj->freq));
    obj->MQ.expire_time = params->vtime + params->lifetime;

    MQ_adjust(cache);
  } else {
    /* cache miss: check the history buffer */
    params->hit_on_ghost = false;
    params->ghost_freq = 0;
    params->ghost_obj_id = 0;
    cache_obj_t *ghost_obj = params->Qout->find(params->Qout, req, false);
    if (ghost_obj != NULL) {
      params->hit_on_ghost = true;
      params->ghost_freq = ghost_obj->freq;
      params->ghost_obj_id = req->obj_id;
      params->Qout->remove(params->Qout, req->obj_id);
    }
  }

  return obj;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * eviction should be performed before calling this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
static cache_obj_t *MQ_insert(cache_t *cache, const request_t *req) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);

  if (params->hit_on_ghost && params->ghost_obj_id == req->obj_id) {
    /* restore the remembered frequency from the history buffer */
    obj->freq = params->ghost_freq;
    if (obj->freq < INT32_MAX) {
      obj->freq++;
    }
  } else {
    obj->freq = 1;
  }
  params->hit_on_ghost = false;
  params->ghost_freq = 0;
  params->ghost_obj_id = 0;

  MQ_queue_link(cache, obj, MQ_queue_num(params, obj->freq));
  obj->MQ.expire_time = params->vtime + params->lifetime;

  MQ_adjust(cache);

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
static cache_obj_t *MQ_to_evict(cache_t *cache, const request_t *req) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  /* the LRU end of the lowest non-empty queue */
  for (int i = 0; i < params->n_queues; i++) {
    if (params->queue_tails[i] != NULL) {
      return params->queue_tails[i];
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
 */
static void MQ_evict(cache_t *cache, const request_t *req) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;

  cache_obj_t *obj = MQ_to_evict(cache, req);
  DEBUG_ASSERT(obj != NULL);

  /* remember the evicted object's id and frequency in the history buffer */
  int32_t evicted_freq = obj->freq;
  copy_cache_obj_to_request(params->req_local, obj);
  params->Qout->get(params->Qout, params->req_local);
  cache_obj_t *ghost_obj =
      params->Qout->find(params->Qout, params->req_local, false);
  if (ghost_obj != NULL) {
    ghost_obj->freq = evicted_freq;
  }

  MQ_queue_unlink(cache, obj);

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
static bool MQ_remove(cache_t *cache, obj_id_t obj_id) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;

  /* also remove from the history buffer */
  params->Qout->remove(params->Qout, obj_id);

  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  MQ_queue_unlink(cache, obj);

  cache_remove_obj_base(cache, obj, true);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                       internal functions                      ****
// ****                                                               ****
// ***********************************************************************

/**
 * @brief insert obj at the MRU end of the given queue and update bookkeeping
 */
static inline void MQ_queue_link(cache_t *cache, cache_obj_t *obj, int queue) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  DEBUG_ASSERT(queue >= 0 && queue < params->n_queues);
  prepend_obj_to_head(&params->queue_heads[queue], &params->queue_tails[queue],
                      obj);
  params->queue_n_bytes[queue] += obj->obj_size + cache->obj_md_size;
  params->queue_n_objs[queue]++;
  obj->MQ.queue_id = queue;
}

/**
 * @brief remove obj from the queue recorded in its metadata and update
 * bookkeeping
 */
static inline void MQ_queue_unlink(cache_t *cache, cache_obj_t *obj) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  int queue = obj->MQ.queue_id;
  DEBUG_ASSERT(queue >= 0 && queue < params->n_queues);
  DEBUG_ASSERT(params->queue_n_objs[queue] > 0);
  remove_obj_from_list(&params->queue_heads[queue], &params->queue_tails[queue],
                       obj);
  params->queue_n_bytes[queue] -= obj->obj_size + cache->obj_md_size;
  params->queue_n_objs[queue]--;
}

/**
 * @brief map an access frequency to a queue index:
 * QueueNum(f) = min(floor(log2(f)), n_queues - 1)
 */
static inline int MQ_queue_num(const MQ_params_t *params, int32_t freq) {
  int queue = 0;
  while (freq > 1 && queue < params->n_queues - 1) {
    freq >>= 1;
    queue++;
  }
  return queue;
}

/**
 * @brief the Adjust routine from the MQ paper:
 * increment the virtual time and demote the LRU-end object of each queue
 * to the next lower queue if its lifetime in the current queue has expired
 */
static void MQ_adjust(cache_t *cache) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;

  params->vtime++;
  for (int k = 1; k < params->n_queues; k++) {
    cache_obj_t *obj = params->queue_tails[k];
    if (obj != NULL && obj->MQ.expire_time < params->vtime) {
      MQ_queue_unlink(cache, obj);
      MQ_queue_link(cache, obj, k - 1);
      obj->MQ.expire_time = params->vtime + params->lifetime;
    }
  }
}

// ***********************************************************************
// ****                                                               ****
// ****                  parameter set up functions                   ****
// ****                                                               ****
// ***********************************************************************
static const char *MQ_current_params(MQ_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-queue=%d,lifetime=%ld,Qout-size-ratio=%.2lf\n",
           params->n_queues, (long)params->lifetime, params->Qout_size_ratio);
  return params_str;
}

static void MQ_parse_params(cache_t *cache, const char *cache_specific_params) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    if (key == NULL) {
      ERROR("%s: invalid parameter string\n", cache->cache_name);
    }
    if (value == NULL && strcasecmp(key, "print") != 0) {
      ERROR("%s: parameter \"%s\" has no value\n", cache->cache_name, key);
    }

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "n-queue") == 0) {
      params->n_queues = (int)strtol(value, &end, 10);
      if (strlen(end) > 0) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
      if (params->n_queues < 1 || params->n_queues > MQ_MAX_N_QUEUE) {
        ERROR("n-queue must be between 1 and %d\n", MQ_MAX_N_QUEUE);
      }
    } else if (strcasecmp(key, "lifetime") == 0) {
      params->lifetime = (int64_t)strtoll(value, &end, 10);
      if (strlen(end) > 0) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
      if (params->lifetime < 1) {
        ERROR("lifetime must be positive\n");
      }
    } else if (strcasecmp(key, "Qout-size-ratio") == 0) {
      params->Qout_size_ratio = strtod(value, &end);
      if (end == value || *end != '\0') {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
      if (params->Qout_size_ratio <= 0 || params->Qout_size_ratio > 64) {
        ERROR("Qout-size-ratio must be in (0, 64]\n");
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", MQ_current_params(params));
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
