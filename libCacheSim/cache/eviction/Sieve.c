

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/cache.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USE_BELADY
// #undef USE_BELADY

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  cache_obj_t *pointer;

#ifdef USE_BELADY
  int64_t n_miss;
#endif

  // tracking stats for hand position / retention ratio plotting
  int64_t n_obj_examined_interval;
  int64_t n_obj_retained_interval;
  int64_t n_evictions_interval;
  FILE *tracking_file;
  int64_t last_report_vtime;

  // position-binned retention tracking
#define SIEVE_N_POS_BINS 20
  int64_t examined_per_bin[20];
  int64_t retained_per_bin[20];
  FILE *binned_file;
} Sieve_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
static void Sieve_free(cache_t *cache);
static bool Sieve_get(cache_t *cache, const request_t *req);
static cache_obj_t *Sieve_find(cache_t *cache, const request_t *req,
                               bool update_cache);
static cache_obj_t *Sieve_insert(cache_t *cache, const request_t *req);
static cache_obj_t *Sieve_to_evict(cache_t *cache, const request_t *req);
static void Sieve_evict(cache_t *cache, const request_t *req);
static bool Sieve_remove(cache_t *cache, obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *Sieve_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("Sieve", ccache_params, cache_specific_params);
  cache->cache_init = Sieve_init;
  cache->cache_free = Sieve_free;
  cache->get = Sieve_get;
  cache->find = Sieve_find;
  cache->insert = Sieve_insert;
  cache->evict = Sieve_evict;
  cache->remove = Sieve_remove;
  cache->to_evict = Sieve_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 1;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc(Sieve_params_t);
  memset(cache->eviction_params, 0, sizeof(Sieve_params_t));
  Sieve_params_t *params = (Sieve_params_t *)cache->eviction_params;
  params->pointer = NULL;
  params->q_head = NULL;
  params->q_tail = NULL;

#ifdef USE_BELADY
  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "Sieve_Belady");
#endif

  // open tracking file
  {
    char fname[256];
    const char *trace_name = getenv("TRACKING_TRACE_NAME");
    if (trace_name)
      snprintf(fname, sizeof(fname), "tracking_%s_%s_%lld.csv", trace_name,
               cache->cache_name, (long long)cache->cache_size);
    else
      snprintf(fname, sizeof(fname), "tracking_%s_%lld.csv",
               cache->cache_name, (long long)cache->cache_size);
    params->tracking_file = fopen(fname, "w");
    if (params->tracking_file) {
      fprintf(params->tracking_file, "vtime,n_obj,avg_scan_depth,retention_ratio,hand_pos\n");
    }

    // open binned retention file
    char bfname[256];
    if (trace_name)
      snprintf(bfname, sizeof(bfname), "tracking_%s_%s_%lld_binned.csv",
               trace_name, cache->cache_name, (long long)cache->cache_size);
    else
      snprintf(bfname, sizeof(bfname), "tracking_%s_%lld_binned.csv",
               cache->cache_name, (long long)cache->cache_size);
    params->binned_file = fopen(bfname, "w");
    if (params->binned_file) {
      fprintf(params->binned_file, "vtime");
      for (int b = 0; b < SIEVE_N_POS_BINS; b++)
        fprintf(params->binned_file, ",ret_%d", b * 5);
      fprintf(params->binned_file, "\n");
    }
  }

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void Sieve_free(cache_t *cache) {
  Sieve_params_t *params = (Sieve_params_t *)cache->eviction_params;
  if (params->tracking_file) fclose(params->tracking_file);
  if (params->binned_file) fclose(params->binned_file);
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

static bool Sieve_get(cache_t *cache, const request_t *req) {
  bool ck_hit = cache_get_base(cache, req);
#ifdef USE_BELADY
  if (!ck_hit) {
    Sieve_params_t *params = cache->eviction_params;
    params->n_miss++;
  }
#endif
  return ck_hit;
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
static cache_obj_t *Sieve_find(cache_t *cache, const request_t *req,
                               bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);
  if (cache_obj != NULL && update_cache) {
    cache_obj->sieve.freq = 1;
  }

  return cache_obj;
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
static cache_obj_t *Sieve_insert(cache_t *cache, const request_t *req) {
  Sieve_params_t *params = cache->eviction_params;
  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);
  obj->sieve.freq = 0;

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
static cache_obj_t *Sieve_to_evict_with_freq(cache_t *cache,
                                             const request_t *req,
                                             int to_evict_freq) {
  Sieve_params_t *params = cache->eviction_params;
  cache_obj_t *pointer = params->pointer;

  /* if we have run one full around or first eviction */
  if (pointer == NULL) pointer = params->q_tail;

  /* find the first untouched */
  while (pointer != NULL && pointer->sieve.freq > to_evict_freq) {
    pointer = pointer->queue.prev;
  }

  /* if we have finished one around, start from the tail */
  if (pointer == NULL) {
    pointer = params->q_tail;
    while (pointer != NULL && pointer->sieve.freq > to_evict_freq) {
      pointer = pointer->queue.prev;
    }
  }

  if (pointer == NULL) return NULL;

  return pointer;
}

static cache_obj_t *Sieve_to_evict(cache_t *cache, const request_t *req) {
  // because we do not change the frequency of the object,
  // if all objects have frequency 1, we may return NULL
  int to_evict_freq = 0;

  cache_obj_t *obj_to_evict =
      Sieve_to_evict_with_freq(cache, req, to_evict_freq);

  while (obj_to_evict == NULL) {
    to_evict_freq += 1;

    obj_to_evict = Sieve_to_evict_with_freq(cache, req, to_evict_freq);
  }

  return obj_to_evict;
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
#ifdef USE_BELADY
static inline bool Sieve_should_retain(cache_t *cache, cache_obj_t *obj) {
  Sieve_params_t *params = cache->eviction_params;

  // if (obj->sieve.freq == 0) return false;

  if (obj->next_access_vtime == -1 || obj->next_access_vtime == INT64_MAX) {
    return false;
  }

  double miss_ratio = (double)params->n_miss / (double)cache->n_req;
  int64_t next_access_dist = obj->next_access_vtime - cache->n_req;
  int64_t thresh = (int64_t)((double)cache->cache_size / miss_ratio);
  if (next_access_dist > thresh) {
    return false;
  }
  return true;
}
#endif

static void Sieve_evict(cache_t *cache, const request_t *req) {
  Sieve_params_t *params = cache->eviction_params;
  int64_t n_obj = cache->get_n_obj(cache);

  /* if we have run one full around or first eviction */
  cache_obj_t *obj = params->pointer == NULL ? params->q_tail : params->pointer;

  // compute distance from current obj to tail for position binning
  int64_t dist_to_tail = 0;
  {
    cache_obj_t *p = obj;
    while (p->queue.next != NULL) {
      dist_to_tail++;
      p = p->queue.next;
    }
  }

#ifdef USE_BELADY
  while (Sieve_should_retain(cache, obj)) {
#else
  while (obj->sieve.freq > 0) {
#endif
    // track position-binned retention
    if (params->binned_file && n_obj > 0) {
      int bin = (int)((double)dist_to_tail / n_obj * SIEVE_N_POS_BINS);
      if (bin >= SIEVE_N_POS_BINS) bin = SIEVE_N_POS_BINS - 1;
      params->examined_per_bin[bin]++;
      params->retained_per_bin[bin]++;
    }

    obj->sieve.freq -= 1;
    obj = obj->queue.prev == NULL ? params->q_tail : obj->queue.prev;
    // update distance: prev goes toward head (further from tail)
    // wrap to tail resets to 0
    if (obj == params->q_tail)
      dist_to_tail = 0;
    else
      dist_to_tail++;

    params->n_obj_examined_interval++;
    params->n_obj_retained_interval++;
  }

  // the evicted object
  if (params->binned_file && n_obj > 0) {
    int bin = (int)((double)dist_to_tail / n_obj * SIEVE_N_POS_BINS);
    if (bin >= SIEVE_N_POS_BINS) bin = SIEVE_N_POS_BINS - 1;
    params->examined_per_bin[bin]++;
  }

  params->n_obj_examined_interval++;
  params->n_evictions_interval++;

  params->pointer = obj->queue.prev;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_evict_base(cache, obj, true);

  if (params->tracking_file &&
      cache->n_req - params->last_report_vtime >= 100000) {
    double avg_scan = params->n_evictions_interval > 0
        ? (double)params->n_obj_examined_interval / params->n_evictions_interval
        : 0.0;
    double retention = params->n_obj_examined_interval > 0
        ? (double)params->n_obj_retained_interval / params->n_obj_examined_interval
        : 0.0;
    // compute hand position: walk from pointer to tail
    double hand_pos = 0.0;
    int64_t cur_n_obj = cache->get_n_obj(cache);
    if (cur_n_obj > 0 && params->pointer != NULL) {
      int64_t pos = 0;
      cache_obj_t *p = params->pointer;
      while (p->queue.next != NULL) {
        pos++;
        p = p->queue.next;
      }
      hand_pos = (double)pos / cur_n_obj;
    }
    fprintf(params->tracking_file, "%ld,%ld,%.4f,%.4f,%.4f\n",
            (long)cache->n_req, (long)cur_n_obj,
            avg_scan, retention, hand_pos);
    params->n_obj_examined_interval = 0;
    params->n_obj_retained_interval = 0;
    params->n_evictions_interval = 0;
    params->last_report_vtime = cache->n_req;

    // dump binned retention
    if (params->binned_file) {
      fprintf(params->binned_file, "%ld", (long)cache->n_req);
      for (int b = 0; b < SIEVE_N_POS_BINS; b++) {
        double bin_ret = params->examined_per_bin[b] > 0
            ? (double)params->retained_per_bin[b] / params->examined_per_bin[b]
            : -1.0;
        fprintf(params->binned_file, ",%.4f", bin_ret);
      }
      fprintf(params->binned_file, "\n");
      memset(params->examined_per_bin, 0, sizeof(params->examined_per_bin));
      memset(params->retained_per_bin, 0, sizeof(params->retained_per_bin));
    }
  }
}

static void Sieve_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  DEBUG_ASSERT(obj_to_remove != NULL);
  Sieve_params_t *params = cache->eviction_params;
  if (obj_to_remove == params->pointer) {
    params->pointer = obj_to_remove->queue.prev;
  }
  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_remove);
  cache_remove_obj_base(cache, obj_to_remove, true);
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
static bool Sieve_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  Sieve_remove_obj(cache, obj);

  return true;
}

static void Sieve_verify(cache_t *cache) {
  Sieve_params_t *params = cache->eviction_params;
  int64_t n_obj = 0, n_byte = 0;
  cache_obj_t *obj = params->q_head;

  while (obj != NULL) {
    assert(hashtable_find_obj_id(cache->hashtable, obj->obj_id) != NULL);
    n_obj++;
    n_byte += obj->obj_size;
    obj = obj->queue.next;
  }

  assert(n_obj == cache->get_n_obj(cache));
  assert(n_byte == cache->get_occupied_byte(cache));
}

#ifdef __cplusplus
}
#endif
