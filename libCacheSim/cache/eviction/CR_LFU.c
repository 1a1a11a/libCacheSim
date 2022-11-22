
#include "../../include/libCacheSim/evictionAlgo/CR_LFU.h"

#include <glib.h>
#include <math.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/SR_LRU.h"
// CR_LFU is used by Cacheus.

#ifdef __cplusplus
extern "C" {
#endif

static void free_list_node(void *list_node) {
  my_free(sizeof(freq_node_t), list_node);
}

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

cache_t *CR_LFU_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("CR_LFU", ccache_params);
  cache->cache_init = CR_LFU_init;
  cache->cache_free = CR_LFU_free;
  cache->get = CR_LFU_get;
  cache->check = CR_LFU_check;
  cache->insert = CR_LFU_insert;
  cache->evict = CR_LFU_evict;
  cache->remove = CR_LFU_remove;
  cache->to_evict = CR_LFU_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->per_obj_metadata_size = 16;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  if (cache_specific_params != NULL) {
    printf("CR-LFU does not support any parameters, but got %s\n",
           cache_specific_params);
    abort();
  }

  CR_LFU_params_t *params = my_malloc_n(CR_LFU_params_t, 1);
  cache->eviction_params = params;

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

void CR_LFU_free(cache_t *cache) {
  CR_LFU_params_t *params = (CR_LFU_params_t *)(cache->eviction_params);
  g_hash_table_destroy(params->freq_map);
  my_free(sizeof(CR_LFU_params_t), params);
  cache_struct_free(cache);
}

cache_ck_res_e CR_LFU_check(cache_t *cache, const request_t *req,
                            const bool update_cache) {
  cache_obj_t *cache_obj;
  cache_ck_res_e ret = cache_check_base(cache, req, update_cache, &cache_obj);

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
      VVERBOSE("allocate new %d %d %p %p\n", new_node->freq, new_node->n_obj,
               new_node->first_obj, new_node->last_obj);
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
  return ret;
}

cache_ck_res_e CR_LFU_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_obj_t *CR_LFU_insert(cache_t *cache, const request_t *req) {
  CR_LFU_params_t *params = (CR_LFU_params_t *)(cache->eviction_params);
  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  cache_obj->lfu.freq = 1;

  if (params->other_cache) {
    // Check if the requested obj is present in SR-LRU's history
    cache_obj_t *obj_other_cache = cache_get_obj_by_id(
        ((SR_LRU_params_t *)params->other_cache->eviction_params)->H_list,
        cache_obj->obj_id);
    DEBUG_ASSERT(
        cache_get_obj_by_id(
            ((SR_LRU_params_t *)params->other_cache->eviction_params)->R_list,
            cache_obj->obj_id) == NULL);
    DEBUG_ASSERT(
        cache_get_obj_by_id(
            ((SR_LRU_params_t *)params->other_cache->eviction_params)->SR_list,
            cache_obj->obj_id) == NULL);
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
      VVERBOSE("allocate new %d %d %p %p\n", new_node->freq, new_node->n_obj,
               new_node->first_obj, new_node->last_obj);
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
  if (params->min_freq > cache_obj->lfu.freq) {
    params->min_freq = cache_obj->lfu.freq;
  }
  freq_node_t *min_freq_node =
      g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->last_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);

  return cache_obj;
}

cache_obj_t *CR_LFU_to_evict(cache_t *cache) {
  CR_LFU_params_t *params = (CR_LFU_params_t *)(cache->eviction_params);

  freq_node_t *min_freq_node =
      g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->last_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);

  return min_freq_node->last_obj;
}

void CR_LFU_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *evicted_obj) {
  CR_LFU_params_t *params = (CR_LFU_params_t *)(cache->eviction_params);

  freq_node_t *min_freq_node =
      g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->last_obj != NULL);

  DEBUG_ASSERT(min_freq_node->n_obj > 0);
  min_freq_node->n_obj--;

  cache_obj_t *obj_to_evict = min_freq_node->last_obj;

  if (params->other_cache) {
    // Before evicting the object, "offload" the obj frequency to history in
    // SR-LRU In case in the future history hit, LFU can load that frequency
    // again.
    cache_obj_t *obj_other_cache = cache_get_obj_by_id(
        ((SR_LRU_params_t *)params->other_cache->eviction_params)->H_list,
        obj_to_evict->obj_id);
    DEBUG_ASSERT(obj_other_cache != NULL);
    obj_other_cache->CR_LFU.freq = obj_to_evict->lfu.freq;
  }

  if (evicted_obj != NULL)
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));

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
    DEBUG_ASSERT(params->min_freq > old_min_freq);

  } else {
    min_freq_node->last_obj = obj_to_evict->queue.prev;
    obj_to_evict->queue.prev->queue.next = NULL;
  }
  cache_remove_obj_base(cache, obj_to_evict);

  min_freq_node =
      g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->last_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);
}

void CR_LFU_remove(cache_t *cache, const obj_id_t obj_id) {
  CR_LFU_params_t *params = (CR_LFU_params_t *)(cache->eviction_params);
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);

  if (obj == NULL) {
    WARN("obj to remove is not in the cache\n");
    return;
  }

  if (params->other_cache) {
    // Before evicting the object, "offload" the obj frequency to history in
    // SR-LRU In case in the future history hit, LFU can load that frequency
    // again.
    cache_obj_t *obj_other_cache = cache_get_obj_by_id(
        ((SR_LRU_params_t *)params->other_cache->eviction_params)->H_list,
        obj->obj_id);
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

  cache_remove_obj_base(cache, obj);

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
}

#ifdef __cplusplus
}
#endif
