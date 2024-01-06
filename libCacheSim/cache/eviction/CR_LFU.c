
#include <glib.h>
#include <math.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/evictionAlgo/Cacheus.h"
// CR_LFU is used by Cacheus.

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void CR_LFU_parse_params(cache_t *cache,
                                const char *cache_specific_params);
static void CR_LFU_free(cache_t *cache);
static bool CR_LFU_get(cache_t *cache, const request_t *req);
static cache_obj_t *CR_LFU_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *CR_LFU_insert(cache_t *cache, const request_t *req);
static cache_obj_t *CR_LFU_to_evict(cache_t *cache, const request_t *req);
static void CR_LFU_evict(cache_t *cache, const request_t *req);
static bool CR_LFU_remove(cache_t *cache, const obj_id_t obj_id);

static void free_list_node(void *list_node) {
  my_free(sizeof(freq_node_t), list_node);
}

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
 * @param cache_specific_params cache specific parameters, should be NULL
 */
cache_t *CR_LFU_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("CR_LFU", ccache_params, cache_specific_params);
  cache->cache_init = CR_LFU_init;
  cache->cache_free = CR_LFU_free;
  cache->get = CR_LFU_get;
  cache->find = CR_LFU_find;
  cache->insert = CR_LFU_insert;
  cache->evict = CR_LFU_evict;
  cache->remove = CR_LFU_remove;
  cache->to_evict = CR_LFU_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 16;
  } else {
    cache->obj_md_size = 0;
  }

  CR_LFU_params_t *params = my_malloc_n(CR_LFU_params_t, 1);
  cache->eviction_params = params;
  params->req_local = new_request();

  params->min_freq = 1;
  params->max_freq = 1;
  params->other_cache = NULL;  // for Cacheus

  freq_node_t *freq_node = my_malloc_n(freq_node_t, 1);
  params->freq_one_node = freq_node;
  freq_node->freq = 1;
  freq_node->n_obj = 0;
  freq_node->first_obj = NULL;
  freq_node->last_obj = NULL;

  params->freq_map = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
                                           (GDestroyNotify)free_list_node);
  g_hash_table_insert(params->freq_map, GSIZE_TO_POINTER(1), freq_node);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void CR_LFU_free(cache_t *cache) {
  CR_LFU_params_t *params = (CR_LFU_params_t *)(cache->eviction_params);
  free_request(params->req_local);
  g_hash_table_destroy(params->freq_map);
  my_free(sizeof(CR_LFU_params_t), params);
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
static bool CR_LFU_get(cache_t *cache, const request_t *req) {
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
static cache_obj_t *CR_LFU_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);

  if (cache_obj && likely(update_cache)) {
    CR_LFU_params_t *params = (CR_LFU_params_t *)(cache->eviction_params);
    /* freq incr and move to next freq node */
    cache_obj->lfu.freq += 1;
    if (params->max_freq < cache_obj->lfu.freq) {
      params->max_freq = cache_obj->lfu.freq;
    }

    // find the freq_node this object belongs to and update its info
    freq_node_t *old_node = g_hash_table_lookup(
        params->freq_map, GSIZE_TO_POINTER(cache_obj->lfu.freq - 1));
    DEBUG_ASSERT(old_node != NULL);
    DEBUG_ASSERT(old_node->freq == cache_obj->lfu.freq - 1);
    DEBUG_ASSERT(old_node->n_obj > 0);
    old_node->n_obj -= 1;
    remove_obj_from_list(&old_node->first_obj, &old_node->last_obj, cache_obj);

    // find the new freq_node this object should move to
    freq_node_t *new_node = g_hash_table_lookup(
        params->freq_map, GSIZE_TO_POINTER(cache_obj->lfu.freq));
    if (new_node == NULL) {
      new_node = my_malloc_n(freq_node_t, 1);
      memset(new_node, 0, sizeof(freq_node_t));
      new_node->freq = cache_obj->lfu.freq;
      g_hash_table_insert(params->freq_map,
                          GSIZE_TO_POINTER(cache_obj->lfu.freq), new_node);
    } else {
      // it could be new_node is empty
      DEBUG_ASSERT(new_node->freq == cache_obj->lfu.freq);
    }

    /* add to tail of the list */
    if (new_node->last_obj != NULL) {
      new_node->last_obj->queue.next = cache_obj;
      cache_obj->queue.prev = new_node->last_obj;
    } else {
      DEBUG_ASSERT(new_node->first_obj == NULL);
      DEBUG_ASSERT(new_node->n_obj == 0);
      new_node->first_obj = cache_obj;
      cache_obj->queue.prev = NULL;
    }

    cache_obj->queue.next = NULL;
    new_node->last_obj = cache_obj;
    new_node->n_obj += 1;

    // if the old freq_node only has one object, after removing this object, it
    // will have no object and if it is the min_freq, then we should update
    // min_freq
    if (params->min_freq == old_node->freq && old_node->n_obj == 0) {
      /* update min freq */
      uint64_t old_min_freq = params->min_freq;
      for (uint64_t freq = params->min_freq + 1; freq <= params->max_freq;
           freq++) {
        freq_node_t *node =
            g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(freq));
        if (node != NULL && node->n_obj > 0) {
          params->min_freq = freq;
          break;
        }
      }
      DEBUG_ASSERT(params->min_freq > old_min_freq);
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
static cache_obj_t *CR_LFU_insert(cache_t *cache, const request_t *req) {
  CR_LFU_params_t *params = (CR_LFU_params_t *)(cache->eviction_params);
  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  cache_obj->lfu.freq = 1;

  if (params->other_cache) {
    // Check if the requested obj is present in SR-LRU's history
    SR_LRU_params_t *p = params->other_cache->eviction_params;
    cache_obj_t *obj_other_cache = p->H_list->find(p->H_list, req, false);
    DEBUG_ASSERT(p->R_list->find(p->R_list, req, false) == NULL);
    DEBUG_ASSERT(p->SR_list->find(p->SR_list, req, false) == NULL);
    if (obj_other_cache != NULL) {
      DEBUG_ASSERT(obj_other_cache->CR_LFU.freq >= 1);
      // Load the obj frequency into the current CR-LFU
      cache_obj->lfu.freq = obj_other_cache->CR_LFU.freq + 1;
    }
  }

  // If the obj was new, i.e. not inserted in SR_LRU history
  if (cache_obj->lfu.freq == 1) {
    params->min_freq = 1;
    freq_node_t *freq_one_node = params->freq_one_node;
    cache_obj->queue.prev = freq_one_node->last_obj;
    freq_one_node->n_obj += 1;

    if (freq_one_node->last_obj != NULL) {
      freq_one_node->last_obj->queue.next = cache_obj;
    } else {
      DEBUG_ASSERT(freq_one_node->first_obj == NULL);
      freq_one_node->first_obj = cache_obj;
    }
    freq_one_node->last_obj = cache_obj;
    DEBUG_ASSERT(freq_one_node->first_obj != NULL);
  } else {
    // find the new freq_node this object should move to
    DEBUG_ASSERT(params->other_cache != NULL);
    freq_node_t *new_node = g_hash_table_lookup(
        params->freq_map, GSIZE_TO_POINTER(cache_obj->lfu.freq));
    if (new_node == NULL) {
      new_node = my_malloc_n(freq_node_t, 1);
      memset(new_node, 0, sizeof(freq_node_t));
      new_node->freq = cache_obj->lfu.freq;
      g_hash_table_insert(params->freq_map,
                          GSIZE_TO_POINTER(cache_obj->lfu.freq), new_node);
    } else {
      // it could be new_node is empty
      DEBUG_ASSERT(new_node->freq == cache_obj->lfu.freq);
    }
    /* add to tail of the list */
    if (new_node->last_obj != NULL) {
      new_node->last_obj->queue.next = cache_obj;
      cache_obj->queue.prev = new_node->last_obj;
    } else {
      DEBUG_ASSERT(new_node->first_obj == NULL);
      DEBUG_ASSERT(new_node->n_obj == 0);
      new_node->first_obj = cache_obj;
      cache_obj->queue.prev = NULL;
    }

    cache_obj->queue.next = NULL;
    new_node->last_obj = cache_obj;
    new_node->n_obj += 1;
  }

  // Update min_freq and max_freq
  if (params->max_freq < cache_obj->lfu.freq) {
    params->max_freq = cache_obj->lfu.freq;
  }
  if (params->min_freq > cache_obj->lfu.freq || params->min_freq == -1) {
    // when considering object size, it is possible that
    // one object evicts all other objects
    // and it has a frequency more than 1
    // because the history is kept in SR-LRU
    params->min_freq = cache_obj->lfu.freq;
  }

  freq_node_t *min_freq_node =
      g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->last_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);

  return cache_obj;
}

static cache_obj_t *CR_LFU_to_evict(cache_t *cache, const request_t *req) {
  CR_LFU_params_t *params = (CR_LFU_params_t *)(cache->eviction_params);

  freq_node_t *min_freq_node =
      g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->last_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);

  cache->to_evict_candidate = min_freq_node->last_obj;
  cache->to_evict_candidate_gen_vtime = cache->n_req;

  return cache->to_evict_candidate;
}

static void CR_LFU_evict(cache_t *cache, const request_t *req) {
  CR_LFU_params_t *params = (CR_LFU_params_t *)(cache->eviction_params);

  freq_node_t *min_freq_node =
      g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->last_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);

  min_freq_node->n_obj--;
  cache_obj_t *obj_to_evict = min_freq_node->last_obj;
  copy_cache_obj_to_request(params->req_local, obj_to_evict);

  if (params->other_cache) {
    // Before evicting the object, "offload" the obj frequency to history in
    // SR-LRU In case in the future history hit, LFU can load that frequency
    // again.
    SR_LRU_params_t *p = params->other_cache->eviction_params;
    cache_obj_t *obj_other_cache =
        p->H_list->find(p->H_list, params->req_local, false);
    DEBUG_ASSERT(obj_other_cache != NULL);
    obj_other_cache->CR_LFU.freq = obj_to_evict->lfu.freq;
  }

  if (obj_to_evict->queue.prev == NULL) {
    /* the only obj of curr freq */
    DEBUG_ASSERT(min_freq_node->first_obj == obj_to_evict);
    DEBUG_ASSERT(min_freq_node->n_obj == 0);
    min_freq_node->first_obj = NULL;
    min_freq_node->last_obj = NULL;

    /* update min freq */
    uint64_t old_min_freq = params->min_freq;
    for (uint64_t freq = params->min_freq + 1; freq <= params->max_freq;
         freq++) {
      freq_node_t *node =
          g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(freq));
      if (node != NULL && node->n_obj > 0) {
        params->min_freq = freq;
        break;
      }
    }
    if (params->min_freq == old_min_freq) {
      params->min_freq = -1;
      DEBUG_ASSERT(cache->n_obj == 1);
    } else {
      DEBUG_ASSERT(params->min_freq > old_min_freq);
    }

  } else {
    min_freq_node->last_obj = obj_to_evict->queue.prev;
    obj_to_evict->queue.prev->queue.next = NULL;
  }
  cache_remove_obj_base(cache, obj_to_evict, true);

  if (params->min_freq != -1) {
    min_freq_node = g_hash_table_lookup(params->freq_map,
                                        GSIZE_TO_POINTER(params->min_freq));
    DEBUG_ASSERT(min_freq_node != NULL);
    DEBUG_ASSERT(min_freq_node->last_obj != NULL);
    DEBUG_ASSERT(min_freq_node->n_obj > 0);
  }
}

static bool CR_LFU_remove(cache_t *cache, const obj_id_t obj_id) {
  CR_LFU_params_t *params = (CR_LFU_params_t *)(cache->eviction_params);

  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);

  if (obj == NULL) {
    return false;
  }

  copy_cache_obj_to_request(params->req_local, obj);
  if (params->other_cache) {
    // Before evicting the object, "offload" the obj frequency to history in
    // SR-LRU In case in the future history hit, LFU can load that frequency
    // again.
    SR_LRU_params_t *p = params->other_cache->eviction_params;
    cache_obj_t *obj_other_cache =
        p->H_list->find(p->H_list, params->req_local, false);
    // Since we call SR_LRU evict before CR_LFU remove, the obj has to be either
    // in H
    DEBUG_ASSERT(obj_other_cache != NULL);
    obj_other_cache->CR_LFU.freq = obj->lfu.freq;
  }

  freq_node_t *freq_node =
      g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(obj->lfu.freq));
  DEBUG_ASSERT(freq_node->freq == obj->lfu.freq);
  DEBUG_ASSERT(freq_node->n_obj > 0);

  freq_node->n_obj--;
  remove_obj_from_list(&freq_node->first_obj, &freq_node->last_obj, obj);

  cache_remove_obj_base(cache, obj, true);

  freq_node_t *min_freq_node =
      g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(params->min_freq));

  if (min_freq_node->n_obj == 0) {
    /* the only obj of min freq */
    min_freq_node->first_obj = NULL;
    min_freq_node->last_obj = NULL;

    /* update min freq */
    uint64_t old_min_freq = params->min_freq;
    for (uint64_t freq = params->min_freq + 1; freq <= params->max_freq;
         freq++) {
      freq_node_t *node =
          g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(freq));
      if (node != NULL && node->n_obj > 0) {
        params->min_freq = freq;
        break;
      }
    }
    DEBUG_ASSERT(params->min_freq > old_min_freq);
  }

  min_freq_node =
      g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->last_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);

  return true;
}

// ***********************************************************************
// ****                                                               ****
// ****                         debug functions                       ****
// ****                                                               ****
// ***********************************************************************
static int _verify(cache_t *cache) {
  CR_LFU_params_t *CR_LFUDA_params =
      (CR_LFU_params_t *)(cache->eviction_params);
  cache_obj_t *cache_obj, *prev_obj;
  /* update min freq */
  for (uint64_t freq = 1; freq <= CR_LFUDA_params->max_freq; freq++) {
    freq_node_t *freq_node =
        g_hash_table_lookup(CR_LFUDA_params->freq_map, GSIZE_TO_POINTER(freq));
    if (freq_node != NULL) {
      uint32_t n_obj = 0;
      cache_obj = freq_node->first_obj;
      prev_obj = NULL;
      while (cache_obj != NULL) {
        n_obj++;
        DEBUG_ASSERT(cache_obj->lfu.freq == freq);
        DEBUG_ASSERT(cache_obj->queue.prev == prev_obj);
        prev_obj = cache_obj;
        cache_obj = cache_obj->queue.next;
      }
      DEBUG_ASSERT(freq_node->n_obj == n_obj);
    }
  }
  return 0;
}

#ifdef __cplusplus
}
#endif
