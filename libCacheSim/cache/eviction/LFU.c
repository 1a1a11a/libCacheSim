//
//  LFU.c
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

/*
 * LFU implementation in C
 *
 * note that the miss ratio of LFU cpp can be different from LFU
 * this is because of the difference in handling objects of same frequency
 * this implementation uses FIFO to evict objects with the same frequency
 *
 *
 * this module uses linkedList to order requests by frequency,
 * which gives an O(1) time complexity at each request,
 * the drawback of this implementation is the memory usage, because two pointers
 * are associated with each obj_id
 *
 * this implementation do not keep an object's frequency after evicting from
 * cache so objects are inserted with frequency 1
 */

#include <glib.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LFU_params {
  freq_node_t *freq_one_node;
  GHashTable *freq_map;
  uint64_t min_freq;
  uint64_t max_freq;
} LFU_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void LFU_free(cache_t *cache);
static bool LFU_get(cache_t *cache, const request_t *req);
static cache_obj_t *LFU_find(cache_t *cache, const request_t *req,
                             const bool update_cache);
static cache_obj_t *LFU_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LFU_to_evict(cache_t *cache, const request_t *req);
static void LFU_evict(cache_t *cache, const request_t *req);
static bool LFU_remove(cache_t *cache, const obj_id_t obj_id);
static void LFU_remove_obj(cache_t *cache, cache_obj_t *obj);

/* internal functions */
static inline void free_freq_node(void *list_node);
static inline freq_node_t *get_min_freq_node(LFU_params_t *params);
static inline void update_min_freq(LFU_params_t *params);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************
/**
 * @brief initialize a LFU cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params LFU specific parameters, should be NULL
 */
cache_t *LFU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("LFU", ccache_params, cache_specific_params);
  cache->cache_init = LFU_init;
  cache->cache_free = LFU_free;
  cache->get = LFU_get;
  cache->find = LFU_find;
  cache->insert = LFU_insert;
  cache->evict = LFU_evict;
  cache->remove = LFU_remove;
  cache->to_evict = LFU_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  LFU_params_t *params = my_malloc_n(LFU_params_t, 1);
  memset(params, 0, sizeof(LFU_params_t));
  cache->eviction_params = params;

  freq_node_t *freq_node = my_malloc_n(freq_node_t, 1);
  freq_node->freq = 1;
  freq_node->n_obj = 0;
  freq_node->first_obj = NULL;
  freq_node->last_obj = NULL;

  params->freq_map = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
                                           (GDestroyNotify)free_freq_node);
  g_hash_table_insert(params->freq_map, GSIZE_TO_POINTER(1), freq_node);

  params->min_freq = 1;
  params->max_freq = 1;
  params->freq_one_node = freq_node;

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void LFU_free(cache_t *cache) {
  LFU_params_t *params = (LFU_params_t *)(cache->eviction_params);
  g_hash_table_destroy(params->freq_map);
  my_free(sizeof(LFU_params_t), params);
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
static bool LFU_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
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
static cache_obj_t *LFU_find(cache_t *cache, const request_t *req,
                             const bool update_cache) {
  LFU_params_t *params = (LFU_params_t *)(cache->eviction_params);
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (cache_obj && likely(update_cache)) {
    /* freq incr and move to next freq node */
    cache_obj->lfu.freq += 1;
    if (params->max_freq < cache_obj->lfu.freq) {
      params->max_freq = cache_obj->lfu.freq;
    }

    // find the freq_node this object belongs to and update its info
    gpointer old_key = GSIZE_TO_POINTER(cache_obj->lfu.freq - 1);
    freq_node_t *old_node = g_hash_table_lookup(params->freq_map, old_key);
    DEBUG_ASSERT(old_node != NULL);
    DEBUG_ASSERT(old_node->freq == cache_obj->lfu.freq - 1);
    DEBUG_ASSERT(old_node->n_obj > 0);
    old_node->n_obj -= 1;
    remove_obj_from_list(&old_node->first_obj, &old_node->last_obj, cache_obj);

    // find the new freq_node this object should move to
    gpointer new_key = GSIZE_TO_POINTER(cache_obj->lfu.freq);
    freq_node_t *new_node = g_hash_table_lookup(params->freq_map, new_key);
    if (new_node == NULL) {
      new_node = my_malloc_n(freq_node_t, 1);
      memset(new_node, 0, sizeof(freq_node_t));
      new_node->freq = cache_obj->lfu.freq;
      g_hash_table_insert(params->freq_map, new_key, new_node);
      VVVERBOSE("allocate new %ld %d %p %p\n", new_node->freq, new_node->n_obj,
                new_node->first_obj, new_node->last_obj);
    } else {
      // it could be new_node is empty
      DEBUG_ASSERT(new_node->freq == cache_obj->lfu.freq);
    }

    append_obj_to_tail(&new_node->first_obj, &new_node->last_obj, cache_obj);
    new_node->n_obj += 1;

    if (old_node->n_obj == 0) {
      if (params->min_freq == old_node->freq) {
        // if the old freq_node has one object and is the min_freq_node, after
        // removing this object, the freq_node will have no object,
        // then we should update min_freq to the new freq
        update_min_freq(params);
      }

      if (old_node->freq != 1) {
        g_hash_table_remove(params->freq_map, old_key);
      }
    }
  }
  return cache_obj;
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
static cache_obj_t *LFU_insert(cache_t *cache, const request_t *req) {
  LFU_params_t *params = (LFU_params_t *)(cache->eviction_params);
  params->min_freq = 1;
  freq_node_t *freq_one_node = params->freq_one_node;

  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  cache_obj->lfu.freq = 1;
  freq_one_node->n_obj += 1;

  append_obj_to_tail(&freq_one_node->first_obj, &freq_one_node->last_obj,
                     cache_obj);

  return cache_obj;
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
static cache_obj_t *LFU_to_evict(cache_t *cache, const request_t *req) {
  LFU_params_t *params = (LFU_params_t *)(cache->eviction_params);
  freq_node_t *min_freq_node = get_min_freq_node(params);
  return min_freq_node->first_obj;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 */
static void LFU_evict(cache_t *cache, const request_t *req) {
  LFU_params_t *params = (LFU_params_t *)(cache->eviction_params);

  freq_node_t *min_freq_node = get_min_freq_node(params);
  min_freq_node->n_obj--;

  cache_obj_t *obj_to_evict = min_freq_node->first_obj;

  remove_obj_from_list(&min_freq_node->first_obj, &min_freq_node->last_obj,
                       obj_to_evict);
  if (min_freq_node->n_obj == 0) {
    /* the only obj of curr freq */
    DEBUG_ASSERT(min_freq_node->first_obj == NULL);
    DEBUG_ASSERT(min_freq_node->last_obj == NULL);

    // we delay the min_freq update to next eviction
    params->min_freq = 0;
    // /* update min freq */
    // update_min_freq(params);
  }

  cache_evict_base(cache, obj_to_evict, true);
}

void LFU_remove_obj(cache_t *cache, cache_obj_t *obj) {
  assert(obj != NULL);
  LFU_params_t *params = (LFU_params_t *)(cache->eviction_params);

  gpointer key = GSIZE_TO_POINTER(obj->lfu.freq);
  freq_node_t *freq_node = g_hash_table_lookup(params->freq_map, key);
  DEBUG_ASSERT(freq_node->freq == obj->lfu.freq);
  DEBUG_ASSERT(freq_node->n_obj > 0);

  freq_node->n_obj--;
  remove_obj_from_list(&freq_node->first_obj, &freq_node->last_obj, obj);

  cache_remove_obj_base(cache, obj, true);

  if (freq_node->freq == params->min_freq && freq_node->n_obj == 0) {
    /* update min freq */
    update_min_freq(params);
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
bool LFU_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  LFU_remove_obj(cache, obj);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                  cache internal functions                     ****
// ****                                                               ****
// ***********************************************************************
static inline void free_freq_node(void *list_node) {
  my_free(sizeof(freq_node_t), list_node);
}

static inline freq_node_t *get_min_freq_node(LFU_params_t *params) {
  freq_node_t *min_freq_node = NULL;
  if (params->min_freq == 1) {
    min_freq_node = params->freq_one_node;
  } else {
    if (params->min_freq == 0) {
      /* update min freq */
      update_min_freq(params);
    }
    min_freq_node = g_hash_table_lookup(params->freq_map,
                                        GSIZE_TO_POINTER(params->min_freq));
  }

  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->first_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);

  return min_freq_node;
}

static inline void update_min_freq(LFU_params_t *params) {
  uint64_t old_min_freq = params->min_freq;
  for (uint64_t freq = params->min_freq + 1; freq <= params->max_freq; freq++) {
    freq_node_t *node =
        g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(freq));
    if (node != NULL && node->n_obj > 0) {
      params->min_freq = freq;
      break;
    }
  }
  DEBUG_ASSERT(params->min_freq > old_min_freq);
}

#ifdef __cplusplus
}
#endif
