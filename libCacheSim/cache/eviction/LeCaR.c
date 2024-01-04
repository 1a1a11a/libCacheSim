/* LeCaR: HotStorage'18
 *
 * the implementation implements LRU and LFU within LeCaR, it has a better
 * performance, but it is harder to follow. LeCaR0 is a simpler implementation,
 * it has a lower throughput.
 *
 * */

#include <assert.h>
#include <glib.h>
#include <math.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/logging.h"

#ifdef __cplusplus
extern "C" {
#endif

// #define LECAR_USE_BELADY

static const char *DEFAULT_PARAMS = "update-weight=1,lru-weight=0.5";

typedef struct LeCaR_params {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  // used for LFU
  freq_node_t *freq_one_node;
  GHashTable *freq_map;
  uint64_t min_freq;
  uint64_t max_freq;

  // eviction history
  cache_obj_t *ghost_lru_head;
  cache_obj_t *ghost_lru_tail;
  int64_t lru_g_occupied_byte;

  cache_obj_t *ghost_lfu_head;
  cache_obj_t *ghost_lfu_tail;
  int64_t lfu_g_occupied_byte;

  // LeCaR
  double w_lru;
  double w_lfu;
  double lr;  // learning rate
  double dr;  // discount rate
  int64_t n_hit_lru_history;
  int64_t n_hit_lfu_history;
  bool update_weight;
} LeCaR_params_t;

static void free_freq_node(void *list_node) {
  my_free(sizeof(freq_node_t), list_node);
}

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void LeCaR_parse_params(cache_t *cache,
                               const char *cache_specific_params);
static void LeCaR_free(cache_t *cache);
static bool LeCaR_get(cache_t *cache, const request_t *req);
static cache_obj_t *LeCaR_find(cache_t *cache, const request_t *req,
                               const bool update_cache);
static cache_obj_t *LeCaR_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LeCaR_to_evict(cache_t *cache, const request_t *req);
static void LeCaR_evict(cache_t *cache, const request_t *req);
static bool LeCaR_remove(cache_t *cache, const obj_id_t obj_id);

/* internal */
static void verify_ghost_lru_integrity(cache_t *cache, LeCaR_params_t *params);
static inline void update_LFU_min_freq(LeCaR_params_t *params);
static inline freq_node_t *get_min_freq_node(LeCaR_params_t *params);
static inline void remove_obj_from_freq_node(LeCaR_params_t *params,
                                             cache_obj_t *cache_obj);
static inline void insert_obj_info_freq_node(LeCaR_params_t *params,
                                             cache_obj_t *cache_obj);

static void update_weight(cache_t *cache, int64_t t, double *w_update,
                          double *w_no_update);

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
cache_t *LeCaR_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
#ifdef LECAR_USE_BELADY
  cache_t *cache =
      cache_struct_init("LeCaR-Belady", ccache_params, cache_specific_params);
#else
  cache_t *cache =
      cache_struct_init("LeCaR", ccache_params, cache_specific_params);
#endif
  cache->cache_init = LeCaR_init;
  cache->cache_free = LeCaR_free;
  cache->get = LeCaR_get;
  cache->find = LeCaR_find;
  cache->insert = LeCaR_insert;
  cache->evict = LeCaR_evict;
  cache->remove = LeCaR_remove;
  cache->to_evict = LeCaR_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2 + 8 * 2 + 8;  // LRU chain, LFU chain, history
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc_n(LeCaR_params_t, 1);
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);
  memset(params, 0, sizeof(LeCaR_params_t));

  // LeCaR params
  params->lr = 0.45;
  params->dr = pow(0.005, 1.0 / (double)ccache_params.cache_size);
  params->w_lru = params->w_lfu = 0.50;
  params->update_weight = true;
  params->n_hit_lru_history = params->n_hit_lfu_history = 0;

  params->ghost_lru_head = params->ghost_lru_tail = NULL;
  params->ghost_lfu_head = params->ghost_lfu_tail = NULL;
  params->lru_g_occupied_byte = 0;
  params->lfu_g_occupied_byte = 0;
  params->q_head = params->q_tail = NULL;

  if (cache_specific_params != NULL) {
    LeCaR_parse_params(cache, cache_specific_params);
  } else {
    LeCaR_parse_params(cache, DEFAULT_PARAMS);
  }

  // LFU parameters
  params->min_freq = 1;
  params->max_freq = 1;
  freq_node_t *freq_node = my_malloc_n(freq_node_t, 1);
  params->freq_one_node = freq_node;
  memset(freq_node, 0, sizeof(freq_node_t));
  freq_node->freq = 1;
  params->freq_map = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
                                           (GDestroyNotify)free_freq_node);
  g_hash_table_insert(params->freq_map, GSIZE_TO_POINTER(1), freq_node);

  if (!params->update_weight) {
    snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "LeCaR-%.4lflru",
             params->w_lru);
  }

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void LeCaR_free(cache_t *cache) {
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);
  g_hash_table_destroy(params->freq_map);
  my_free(sizeof(LeCaR_params_t), params);
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
bool LeCaR_get(cache_t *cache, const request_t *req) {
  // LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);
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
static cache_obj_t *LeCaR_find(cache_t *cache, const request_t *req,
                               bool update_cache) {
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);

  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (cache_obj == NULL) {
    return NULL;
  }

  if (!update_cache) {
    if (cache_obj->LeCaR.is_ghost) {
      return NULL;
    } else {
      return cache_obj;
    }
  }

  // if it is a ghost object, update its weight
  if (cache_obj->LeCaR.is_ghost) {
    if (cache_obj->LeCaR.evict_expert == 1) {
      // evicted by expert LRU
      params->n_hit_lru_history++;
      int64_t t = cache->n_req - cache_obj->LeCaR.eviction_vtime;
      update_weight(cache, t, &params->w_lru, &params->w_lfu);

      remove_obj_from_list(&params->ghost_lru_head, &params->ghost_lru_tail,
                           cache_obj);
      params->lru_g_occupied_byte -= (cache_obj->obj_size + cache->obj_md_size);
      hashtable_delete(cache->hashtable, cache_obj);
    } else if (cache_obj->LeCaR.evict_expert == 2) {
      // evicted by expert LFU
      params->n_hit_lfu_history++;
      int64_t t = cache->n_req - cache_obj->LeCaR.eviction_vtime;
      update_weight(cache, t, &params->w_lfu, &params->w_lru);

      remove_obj_from_list(&params->ghost_lfu_head, &params->ghost_lfu_tail,
                           cache_obj);
      params->lfu_g_occupied_byte -= (cache_obj->obj_size + cache->obj_md_size);
      hashtable_delete(cache->hashtable, cache_obj);
    } else {
      assert(cache_obj->LeCaR.evict_expert == -1);
      hashtable_delete(cache->hashtable, cache_obj);
      // the two experts both pick this object, do nothing
      ;
    }
    return NULL;
  } else {
    // if it is an cached object, update cache state
    // update LRU chain
    move_obj_to_head(&params->q_head, &params->q_tail, cache_obj);

    // update LFU state
    // it is possible that this is the only object in the cache
    remove_obj_from_freq_node(params, cache_obj);

    /* freq incr and move to next freq node */
    cache_obj->LeCaR.freq += 1;
    if (params->max_freq < cache_obj->LeCaR.freq) {
      params->max_freq = cache_obj->LeCaR.freq;
    }

    insert_obj_info_freq_node(params, cache_obj);
    if (cache->n_obj == 1) {
      update_LFU_min_freq(params);
    }

    /* it is possible that we update freq to a higher freq
     * when remove_obj_from_freq_node */
    if (cache_obj->LeCaR.freq < params->min_freq) {
      params->min_freq = cache_obj->LeCaR.freq;
      VVERBOSE("update min freq to %d\n", (int)params->min_freq);
    }
  }

  if (cache_obj == NULL || cache_obj->LeCaR.is_ghost) {
    return NULL;
  } else {
    return cache_obj;
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
cache_obj_t *LeCaR_insert(cache_t *cache, const request_t *req) {
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);

  VVERBOSE("insert object %lu into cache\n", (unsigned long)req->obj_id);

  // LRU and hash table insert
  cache_obj_t *cache_obj = cache_insert_base(cache, req);

  prepend_obj_to_head(&params->q_head, &params->q_tail, cache_obj);
  cache_obj->LeCaR.freq = 1;
  cache_obj->LeCaR.is_ghost = false;
  cache_obj->LeCaR.evict_expert = 0;
  cache_obj->LeCaR.eviction_vtime = 0;

  // LFU insert
  params->min_freq = 1;
  freq_node_t *freq_one_node = params->freq_one_node;
  freq_one_node->n_obj += 1;
  cache_obj->LeCaR.lfu_prev = freq_one_node->last_obj;

  if (freq_one_node->last_obj != NULL) {
    DEBUG_ASSERT(freq_one_node->first_obj != NULL);
    freq_one_node->last_obj->LeCaR.lfu_next = cache_obj;
  } else {
    DEBUG_ASSERT(freq_one_node->first_obj == NULL);
    freq_one_node->first_obj = cache_obj;
  }
  freq_one_node->last_obj = cache_obj;

  return cache_obj;
}

#ifdef LECAR_USE_BELADY
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
static cache_obj_t *LeCaR_to_evict(cache_t *cache, const request_t *req) {
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);

  cache_obj_t *lru_choice = params->q_tail;

  freq_node_t *min_freq_node = get_min_freq_node(params);
  cache_obj_t *lfu_choice = min_freq_node->first_obj;

  // we divide by 1,000,000 to avoid overflow when next_access_vtime is
  // INT64_MAX
  double lru_belady_metric = lru_choice->misc.next_access_vtime - cache->n_req;
  lru_belady_metric = lru_belady_metric / 1000000.0 * lru_choice->obj_size;
  double lfu_belady_metric = lfu_choice->misc.next_access_vtime - cache->n_req;
  lfu_belady_metric = lfu_belady_metric / 1000000.0 * lfu_choice->obj_size;

  if (lru_belady_metric > lfu_belady_metric) {
    return lru_choice;
  } else {
    return lfu_choice;
  }
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 */
static void LeCaR_evict(cache_t *cache, const request_t *req) {
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);

  cache_obj_t *cache_obj = NULL;

  cache_obj_t *lru_choice = params->q_tail;

  freq_node_t *min_freq_node = get_min_freq_node(params);
  cache_obj_t *lfu_choice = min_freq_node->first_obj;

  // we divide by 1,000,000 to avoid overflow when next_access_vtime is
  // INT64_MAX
  double lru_belady_metric = lru_choice->misc.next_access_vtime - cache->n_req;
  lru_belady_metric = lru_belady_metric / 1000000.0 * lru_choice->obj_size;
  double lfu_belady_metric = lfu_choice->misc.next_access_vtime - cache->n_req;
  lfu_belady_metric = lfu_belady_metric / 1000000.0 * lfu_choice->obj_size;

  if (lru_belady_metric > lfu_belady_metric) {
    cache_obj = lru_choice;
  } else {
    cache_obj = lfu_choice;
  }

  cache_obj->LeCaR.is_ghost = true;
  cache_obj->LeCaR.evict_expert = -1;
  cache_obj->LeCaR.eviction_vtime = cache->n_req;

  // update LRU chain state
  remove_obj_from_list(&params->q_head, &params->q_tail, cache_obj);

  // update LFU chain state
  remove_obj_from_freq_node(params, cache_obj);

  // update cache state
  DEBUG_ASSERT(cache->occupied_byte >= cache_obj->obj_size);
  cache->occupied_byte -= (cache_obj->obj_size + cache->obj_md_size);
  cache->n_obj -= 1;
}

#else
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
static cache_obj_t *LeCaR_to_evict(cache_t *cache, const request_t *req) {
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);

  cache->to_evict_candidate_gen_vtime = cache->n_req;

  double r = ((double)(next_rand() % 100)) / 100.0;
  if (r < params->w_lru) {
    cache->to_evict_candidate = params->q_tail;
  } else {
    freq_node_t *min_freq_node = get_min_freq_node(params);
    cache->to_evict_candidate = min_freq_node->first_obj;
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
 */
void LeCaR_evict(cache_t *cache, const request_t *req) {
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);

  cache_obj_t *lru_candidate = cache->to_evict_candidate = params->q_tail;
  cache_obj_t *lfu_candidate = get_min_freq_node(params)->first_obj;

  cache_obj_t *obj_to_evict = NULL;

  if (cache->to_evict_candidate_gen_vtime == cache->n_req) {
    // we have generated a candidate in to_evict
    obj_to_evict = cache->to_evict_candidate;
    cache->to_evict_candidate_gen_vtime = -1;

    if (lru_candidate == lfu_candidate) {
      assert(obj_to_evict == lru_candidate);
      obj_to_evict->LeCaR.evict_expert = -1;

    } else if (obj_to_evict == lru_candidate) {
      // evicted from LRU
      obj_to_evict->LeCaR.evict_expert = 1;

    } else {
      // mark as ghost object
      obj_to_evict->LeCaR.evict_expert = 2;
    }

  } else {
    if (lru_candidate == lfu_candidate) {
      obj_to_evict = lru_candidate;
      obj_to_evict->LeCaR.evict_expert = -1;
    } else {
      double r = ((double)(next_rand() % 100)) / 100.0;
      if (r < params->w_lru) {
        obj_to_evict = lru_candidate;
        obj_to_evict->LeCaR.evict_expert = 1;
      } else {
        obj_to_evict = lfu_candidate;
        obj_to_evict->LeCaR.evict_expert = 2;
      }
    }
  }

  obj_to_evict->LeCaR.is_ghost = true;
  obj_to_evict->LeCaR.eviction_vtime = cache->n_req;

  // update LRU chain state
  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_evict);

  // update LFU chain state
  remove_obj_from_freq_node(params, obj_to_evict);

  // update cache state
  cache_evict_base(cache, obj_to_evict, false);

  // update history
  if (obj_to_evict->LeCaR.evict_expert == 1) {
    prepend_obj_to_head(&params->ghost_lru_head, &params->ghost_lru_tail,
                        obj_to_evict);
    params->lru_g_occupied_byte += obj_to_evict->obj_size + cache->obj_md_size;
    // evict ghost entries if its full
    while (params->lru_g_occupied_byte > cache->cache_size / 2) {
      cache_obj_t *ghost_to_evict = params->ghost_lru_tail;
      DEBUG_ASSERT(ghost_to_evict != NULL);
      params->lru_g_occupied_byte -=
          (ghost_to_evict->obj_size + cache->obj_md_size);
      remove_obj_from_list(&params->ghost_lru_head, &params->ghost_lru_tail,
                           ghost_to_evict);
      hashtable_delete(cache->hashtable, ghost_to_evict);
    }
  } else if (obj_to_evict->LeCaR.evict_expert == 2) {
    prepend_obj_to_head(&params->ghost_lfu_head, &params->ghost_lfu_tail,
                        obj_to_evict);
    params->lfu_g_occupied_byte += obj_to_evict->obj_size + cache->obj_md_size;
    // evict ghost entries if its full
    while (params->lfu_g_occupied_byte > cache->cache_size / 2) {
      cache_obj_t *ghost_to_evict = params->ghost_lfu_tail;
      DEBUG_ASSERT(ghost_to_evict != NULL);
      params->lfu_g_occupied_byte -=
          (ghost_to_evict->obj_size + cache->obj_md_size);
      remove_obj_from_list(&params->ghost_lfu_head, &params->ghost_lfu_tail,
                           ghost_to_evict);
      hashtable_delete(cache->hashtable, ghost_to_evict);
    }
  } else {
    // evicted by both caches
    // TODO: this currently does not increase ghost size
  }
}
#endif

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
bool LeCaR_remove(cache_t *cache, obj_id_t obj_id) {
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  // remove from LRU list
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);

  // remove from LFU
  remove_obj_from_freq_node(params, obj);

  // remove from hash table and update cache state
  cache_remove_obj_base(cache, obj, true);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                  parameter set up functions                   ****
// ****                                                               ****
// ***********************************************************************
static const char *LeCaR_current_params(cache_t *cache,
                                        LeCaR_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "update-weight=%d,lru-weight=%.lf",
           params->update_weight, params->w_lru);

  return params_str;
}

static void LeCaR_parse_params(cache_t *cache,
                               const char *cache_specific_params) {
  LeCaR_params_t *params = (LeCaR_params_t *)cache->eviction_params;
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

    if (strcasecmp(key, "update-weight") == 0) {
      if ((int)strtol(value, &end, 0) == 1)
        params->update_weight = true;
      else
        params->update_weight = false;
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }
    } else if (strcasecmp(key, "lru-weight") == 0) {
      params->w_lru = (double)strtod(value, &end);
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", LeCaR_current_params(cache, params));
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
// ****                         internal functions                    ****
// ****                                                               ****
// ***********************************************************************

/* LFU related function, LFU uses a chain of freq node sorted by freq in
 * ascending order, each node stores a list of objects with the same
 * frequency in FIFO order, when evicting, we find the min freq node and
 * evict the first object of the min freq, this is only called when current
 * min_freq is empty
 */
static inline void update_LFU_min_freq(LeCaR_params_t *params) {
  unsigned old_min_freq = params->min_freq;
  for (uint64_t freq = params->min_freq + 1; freq <= params->max_freq; freq++) {
    freq_node_t *node =
        g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(freq));
    if (node != NULL && node->n_obj > 0) {
      params->min_freq = freq;
      break;
    }
  }
  VVERBOSE("update LFU min freq from %u to %u\n", (unsigned)old_min_freq,
           (unsigned)params->min_freq);
  // if the object is the only object in the cache, we may have min_freq == 1
  DEBUG_ASSERT(params->min_freq > old_min_freq ||
               params->q_head == params->q_tail);
}

static inline freq_node_t *get_min_freq_node(LeCaR_params_t *params) {
  freq_node_t *min_freq_node = NULL;
  if (params->min_freq == 1) {
    min_freq_node = params->freq_one_node;
  } else {
    min_freq_node = g_hash_table_lookup(params->freq_map,
                                        GSIZE_TO_POINTER(params->min_freq));
  }

  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->first_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);

  return min_freq_node;
}

static inline void remove_obj_from_freq_node(LeCaR_params_t *params,
                                             cache_obj_t *cache_obj) {
  freq_node_t *freq_node = g_hash_table_lookup(
      params->freq_map, GSIZE_TO_POINTER(cache_obj->LeCaR.freq));
  DEBUG_ASSERT(freq_node != NULL);
  DEBUG_ASSERT(freq_node->freq == cache_obj->LeCaR.freq);
  DEBUG_ASSERT(freq_node->n_obj > 0);
  VVERBOSE("remove object from freq node %p (freq %ld, %u obj)\n", freq_node,
           freq_node->freq, freq_node->n_obj);
  freq_node->n_obj--;

  if (cache_obj == freq_node->first_obj) {
    VVVERBOSE("remove object from freq node --- object is the first object\n");
    freq_node->first_obj = cache_obj->LeCaR.lfu_next;
    if (cache_obj->LeCaR.lfu_next != NULL)
      ((cache_obj_t *)(cache_obj->LeCaR.lfu_next))->LeCaR.lfu_prev = NULL;
  }

  if (cache_obj == freq_node->last_obj) {
    VVVERBOSE("remove object from freq node --- object is the last object\n");
    freq_node->last_obj = cache_obj->LeCaR.lfu_prev;
    if (cache_obj->LeCaR.lfu_prev != NULL)
      ((cache_obj_t *)(cache_obj->LeCaR.lfu_prev))->LeCaR.lfu_next = NULL;
  }

  if (cache_obj->LeCaR.lfu_prev != NULL)
    ((cache_obj_t *)(cache_obj->LeCaR.lfu_prev))->LeCaR.lfu_next =
        cache_obj->LeCaR.lfu_next;

  if (cache_obj->LeCaR.lfu_next != NULL)
    ((cache_obj_t *)(cache_obj->LeCaR.lfu_next))->LeCaR.lfu_prev =
        cache_obj->LeCaR.lfu_prev;

  cache_obj->LeCaR.lfu_prev = NULL;
  cache_obj->LeCaR.lfu_next = NULL;

  if (freq_node->freq == params->min_freq && freq_node->n_obj == 0) {
    update_LFU_min_freq(params);
  }
}

static inline void insert_obj_info_freq_node(LeCaR_params_t *params,
                                             cache_obj_t *cache_obj) {
  // find the new freq_node this object should move to
  freq_node_t *new_node = g_hash_table_lookup(
      params->freq_map, GSIZE_TO_POINTER(cache_obj->LeCaR.freq));
  if (new_node == NULL) {
    new_node = my_malloc_n(freq_node_t, 1);
    memset(new_node, 0, sizeof(freq_node_t));
    new_node->freq = cache_obj->LeCaR.freq;
    g_hash_table_insert(params->freq_map,
                        GSIZE_TO_POINTER(cache_obj->LeCaR.freq), new_node);
  } else {
    DEBUG_ASSERT(new_node->freq == cache_obj->LeCaR.freq);
  }

  /* add to tail of the list */
  if (new_node->last_obj != NULL) {
    new_node->last_obj->LeCaR.lfu_next = cache_obj;
    cache_obj->LeCaR.lfu_prev = new_node->last_obj;
  } else {
    DEBUG_ASSERT(new_node->first_obj == NULL);
    DEBUG_ASSERT(new_node->n_obj == 0);
    new_node->first_obj = cache_obj;
    cache_obj->LeCaR.lfu_prev = NULL;
  }

  cache_obj->LeCaR.lfu_next = NULL;
  new_node->last_obj = cache_obj;
  new_node->n_obj += 1;
}

static void update_weight(cache_t *cache, int64_t t, double *w_update,
                          double *w_no_update) {
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);
  if (!params->update_weight) return;

  double r = pow(params->dr, (double)t);
  *w_update = *w_update * exp(-params->lr * r) + 1e-10; /* to avoid w was 0 */
  double s = *w_update + *w_no_update + +2e-10;
  *w_update = *w_update / s;
  *w_no_update = (*w_no_update + 1e-10) / s;
  DEBUG_ASSERT(fabs(*w_update + *w_no_update - 1.0) < 0.0001);
}

// ***********************************************************************
// ****                                                               ****
// ****                         debug functions                       ****
// ****                                                               ****
// ***********************************************************************
static void verify_ghost_lru_integrity(cache_t *cache, LeCaR_params_t *params) {
  if (params->ghost_lru_head == NULL) {
    assert(params->ghost_lru_tail == NULL);
    assert(params->lru_g_occupied_byte == 0);
    return;
  } else {
    assert(params->ghost_lru_tail != NULL);
    assert(params->lru_g_occupied_byte > 0);
  }

  int64_t ghost_entry_size = 0;
  cache_obj_t *cur = params->ghost_lru_head;
  while (cur != NULL) {
    ghost_entry_size += (cur->obj_size + cache->obj_md_size);
    if (cur->queue.next == NULL) {
      assert(cur == params->ghost_lru_tail);
    } else {
      assert(cur->queue.next->queue.prev == cur);
    }
    cur = cur->queue.next;
  }

  VVVERBOSE(
      "ghost entry head %p tail %p, "
      "ghost_entry_size from scan = %ld,"
      "lru_g_occupied_byte = %ld\n ",
      params->ghost_lru_head, params->ghost_lru_tail, ghost_entry_size,
      params->lru_g_occupied_byte);

  assert(ghost_entry_size == params->lru_g_occupied_byte);
}

#ifdef __cplusplus
}
#endif
