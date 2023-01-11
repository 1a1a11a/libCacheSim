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
  int64_t ghost_entry_used_size;

  // LeCaR
  double w_lru;
  double w_lfu;
  double lr;  // learning rate
  double dr;  // discount rate
  int64_t n_hit_lru_history;
  int64_t n_hit_lfu_history;
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
  cache_t *cache = cache_struct_init("LeCaR-Belady", ccache_params);
#else
  cache_t *cache = cache_struct_init("LeCaR", ccache_params);
#endif
  cache->cache_init = LeCaR_init;
  cache->cache_free = LeCaR_free;
  cache->get = LeCaR_get;
  cache->find = LeCaR_find;
  cache->insert = LeCaR_insert;
  cache->evict = LeCaR_evict;
  cache->remove = LeCaR_remove;
  cache->to_evict = LeCaR_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2 + 8 * 2 + 8;  // LRU chain, LFU chain, history
  } else {
    cache->obj_md_size = 0;
  }

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  cache->eviction_params = my_malloc_n(LeCaR_params_t, 1);
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);
  memset(params, 0, sizeof(LeCaR_params_t));

  // LeCaR params
  params->lr = 0.45;
  params->dr = pow(0.005, 1.0 / (double)ccache_params.cache_size);
  params->w_lru = params->w_lfu = 0.50;
  params->n_hit_lru_history = params->n_hit_lfu_history = 0;

  params->ghost_lru_head = params->ghost_lru_tail = NULL;
  params->q_head = params->q_tail = NULL;

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

  bool is_ghost = cache_obj->LeCaR.ghost_evicted_by_lru ||
                  cache_obj->LeCaR.ghost_evicted_by_lfu;

  if (!update_cache) {
    if (is_ghost) {
      return NULL;
    } else {
      return cache_obj;
    }
  }

  // if it is a ghost object, update its weight
  if (cache_obj->LeCaR.ghost_evicted_by_lru) {
    params->n_hit_lru_history++;
    int64_t t = cache->n_req - cache_obj->LeCaR.eviction_vtime;
    update_weight(cache, t, &params->w_lru, &params->w_lfu);

    remove_obj_from_list(&params->ghost_lru_head, &params->ghost_lru_tail,
                         cache_obj);
    params->ghost_entry_used_size -= (cache_obj->obj_size + cache->obj_md_size);
    hashtable_delete(cache->hashtable, cache_obj);

    return NULL;

  } else if (cache_obj->LeCaR.ghost_evicted_by_lfu) {
    params->n_hit_lfu_history++;
    int64_t t = cache->n_req - cache_obj->LeCaR.eviction_vtime;
    update_weight(cache, t, &params->w_lfu, &params->w_lru);

    remove_obj_from_list(&params->ghost_lru_head, &params->ghost_lru_tail,
                         cache_obj);
    params->ghost_entry_used_size -= (cache_obj->obj_size + cache->obj_md_size);
    hashtable_delete(cache->hashtable, cache_obj);

    return NULL;

  } else {
    // if it is an cached object, update cache state
    // update LRU chain
    move_obj_to_head(&params->q_head, &params->q_tail, cache_obj);

    // update LFU state
    remove_obj_from_freq_node(params, cache_obj);

    /* freq incr and move to next freq node */
    cache_obj->LeCaR.freq += 1;
    if (params->max_freq < cache_obj->LeCaR.freq) {
      params->max_freq = cache_obj->LeCaR.freq;
    }

    insert_obj_info_freq_node(params, cache_obj);

    /* it is possible that we update freq to a higher freq
     * when remove_obj_from_freq_node */
    if (cache_obj->LeCaR.freq < params->min_freq) {
      params->min_freq = cache_obj->LeCaR.freq;
      VVERBOSE("update min freq to %d\n", (int)params->min_freq);
    }

#ifdef LECAR_USE_BELADY
    cache_obj->LeCaR.next_access_vtime = req->next_access_vtime;
#endif
  }

  if (cache_obj == NULL || is_ghost) {
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
  cache_obj->LeCaR.eviction_vtime = 0;
  cache_obj->LeCaR.ghost_evicted_by_lru = false;
  cache_obj->LeCaR.ghost_evicted_by_lfu = false;
#ifdef LECAR_USE_BELADY
  cache_obj->LeCaR.next_access_vtime = req->next_access_vtime;
#endif

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

  cache_obj_t *lru_choice = cache->q_tail;

  freq_node_t *min_freq_node = get_min_freq_node(params);
  cache_obj_t *lfu_choice = min_freq_node->first_obj;

  if (lru_choice->LeCaR.next_access_vtime == -1) {
    return lru_choice;
  } else if (lfu_choice->LeCaR.next_access_vtime == -1) {
    return lfu_choice;
  } else if (lru_choice->LeCaR.next_access_vtime >
             lfu_choice->LeCaR.next_access_vtime) {
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

  cache_obj_t *lru_choice = cache->q_tail;

  freq_node_t *min_freq_node = get_min_freq_node(params);
  cache_obj_t *lfu_choice = min_freq_node->first_obj;

  // we divide by 1,000,000 to avoid overflow when next_access_vtime is
  // INT64_MAX
  double lru_belady_metric =
      (double)(lru_choice->LeCaR.next_access_vtime - cache->n_req) / 1000000.0 *
      lru_choice->obj_size;
  double lfu_belady_metric =
      (double)(lfu_choice->LeCaR.next_access_vtime - cache->n_req) / 1000000.0 *
      lfu_choice->obj_size;
  if (lru_belady_metric > lfu_belady_metric) {
    cache_obj = lru_choice;
    cache_obj->LeCaR.ghost_evicted_by_lru = true;
    cache_obj->LeCaR.ghost_evicted_by_lfu = false;
  } else {
    cache_obj = lfu_choice;
    cache_obj->LeCaR.ghost_evicted_by_lfu = true;
    cache_obj->LeCaR.ghost_evicted_by_lru = false;
  }

  cache_obj->LeCaR.eviction_vtime = cache->n_req;

  // update LRU chain state
  remove_obj_from_list(&cache->q_head, &cache->q_tail, cache_obj);

  // update LFU chain state
  remove_obj_from_freq_node(params, cache_obj);

  // update cache state
  DEBUG_ASSERT(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_metadata_size);
  cache->n_obj -= 1;

  // update history
  if (unlikely(params->ghost_lru_head == NULL)) {
    params->ghost_lru_head = cache_obj;
    params->ghost_lru_tail = cache_obj;
  } else {
    params->ghost_lru_head->queue.prev = cache_obj;
    cache_obj->queue.next = params->ghost_lru_head;
  }
  params->ghost_lru_head = cache_obj;
  VVERBOSE("insert to ghost history, update ghost lru head to %p\n",
           params->ghost_lru_head);

  params->ghost_entry_used_size +=
      cache_obj->obj_size + cache->per_obj_metadata_size;
  // evict ghost entries if its full
  while (params->ghost_entry_used_size > cache->cache_size) {
    cache_obj_t *ghost_to_evict = params->ghost_lru_tail;
    VVERBOSE("remove ghost %p, ghost cache size (before) %ld\n", ghost_to_evict,
             params->ghost_entry_used_size);
    assert(ghost_to_evict != NULL);
    params->ghost_entry_used_size -=
        (ghost_to_evict->obj_size + cache->per_obj_metadata_size);
    params->ghost_lru_tail = params->ghost_lru_tail->queue.prev;
    if (likely(params->ghost_lru_tail != NULL))
      params->ghost_lru_tail->queue.next = NULL;

    hashtable_delete(cache->hashtable, ghost_to_evict);
  }
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

  double r = ((double)(next_rand() % 100)) / 100.0;
  if (r < params->w_lru) {
    return params->q_tail;
  } else {
    freq_node_t *min_freq_node = get_min_freq_node(params);
    return min_freq_node->first_obj;
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
void LeCaR_evict(cache_t *cache, const request_t *req) {
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);

  cache_obj_t *obj_to_evict = NULL;
  double r = ((double)(next_rand() % 100)) / 100.0;
  if (r < params->w_lru) {
    // evict from LRU
    obj_to_evict = params->q_tail;
    VVERBOSE("evict object %lu from LRU\n", (unsigned long)obj_to_evict);

    // mark as ghost object
    obj_to_evict->LeCaR.ghost_evicted_by_lru = true;
    obj_to_evict->LeCaR.ghost_evicted_by_lfu = false;

  } else {
    // evict from LFU
    freq_node_t *min_freq_node = get_min_freq_node(params);
    obj_to_evict = min_freq_node->first_obj;
    VVERBOSE("evict object %lu from LFU\n", (unsigned long)obj_to_evict);

    // mark as ghost object
    obj_to_evict->LeCaR.ghost_evicted_by_lfu = true;
    obj_to_evict->LeCaR.ghost_evicted_by_lru = false;
  }

  obj_to_evict->LeCaR.eviction_vtime = cache->n_req;

  // update LRU chain state
  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_evict);

  // update LFU chain state
  remove_obj_from_freq_node(params, obj_to_evict);

  // update cache state
  DEBUG_ASSERT(cache->occupied_byte >= obj_to_evict->obj_size);
  cache->occupied_byte -= (obj_to_evict->obj_size + cache->obj_md_size);
  cache->n_obj -= 1;

  // update history
  if (unlikely(params->ghost_lru_head == NULL)) {
    params->ghost_lru_head = obj_to_evict;
    params->ghost_lru_tail = obj_to_evict;
  } else {
    params->ghost_lru_head->queue.prev = obj_to_evict;
    obj_to_evict->queue.next = params->ghost_lru_head;
  }
  params->ghost_lru_head = obj_to_evict;
  VVERBOSE("insert to ghost history, update ghost lru head to %p\n",
           params->ghost_lru_head);

  params->ghost_entry_used_size += obj_to_evict->obj_size + cache->obj_md_size;
  // evict ghost entries if its full
  while (params->ghost_entry_used_size > cache->cache_size) {
    cache_obj_t *ghost_to_evict = params->ghost_lru_tail;
    VVERBOSE("remove ghost %p, ghost cache size (before) %ld\n", ghost_to_evict,
             params->ghost_entry_used_size);
    DEBUG_ASSERT(ghost_to_evict != NULL);
    params->ghost_entry_used_size -=
        (ghost_to_evict->obj_size + cache->obj_md_size);
    params->ghost_lru_tail = params->ghost_lru_tail->queue.prev;
    if (likely(params->ghost_lru_tail != NULL))
      params->ghost_lru_tail->queue.next = NULL;

    hashtable_delete(cache->hashtable, ghost_to_evict);
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
  DEBUG_ASSERT(params->min_freq > old_min_freq);
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
  VVERBOSE("remove object from freq node %p (freq %d, %d obj)\n", freq_node,
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
    VVERBOSE(
        "create new freq node (%d): %d object, first obj %p, last object "
        "%p\n",
        new_node->freq, new_node->n_obj, new_node->first_obj,
        new_node->last_obj);
  } else {
    DEBUG_ASSERT(new_node->freq == cache_obj->LeCaR.freq);
  }

  VVERBOSE("insert obj into freq node (freq %u, %u obj)\n", new_node->freq,
           new_node->n_obj);

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
  DEBUG_ASSERT(fabs(*w_update + *w_no_update - 1.0) < 0.0001);
  LeCaR_params_t *params = (LeCaR_params_t *)(cache->eviction_params);

  double r = pow(params->dr, (double)t);
  *w_update = *w_update * exp(-params->lr * r) + 1e-10; /* to avoid w was 0 */
  double s = *w_update + *w_no_update + +2e-10;
  *w_update = *w_update / s;
  *w_no_update = (*w_no_update + 1e-10) / s;
}

// ***********************************************************************
// ****                                                               ****
// ****                         debug functions                       ****
// ****                                                               ****
// ***********************************************************************
static void verify_ghost_lru_integrity(cache_t *cache, LeCaR_params_t *params) {
  if (params->ghost_lru_head == NULL) {
    assert(params->ghost_lru_tail == NULL);
    assert(params->ghost_entry_used_size == 0);
    return;
  } else {
    assert(params->ghost_lru_tail != NULL);
    assert(params->ghost_entry_used_size > 0);
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
      "ghost_entry_used_size = %ld\n ",
      params->ghost_lru_head, params->ghost_lru_tail, ghost_entry_size,
      params->ghost_entry_used_size);

  assert(ghost_entry_size == params->ghost_entry_used_size);
}

#ifdef __cplusplus
}
#endif
