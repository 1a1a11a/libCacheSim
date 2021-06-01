//
//  LFUFast.c
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

/*
 * LFU implementation in C
 *
 * note that the miss ratio of LFU cpp can be different from LFUFast
 * this is because of the difference in handling objects of same frequency
 * LFU fast uses FIFO to evict objects with the same frequency
 *
 *
 * this module uses linkedList to order requests by frequency,
 * which gives an O(1) time complexity at each request,
 * the drawback of this implementation is the memory usage, because two pointers
 * are associated with each obj_id
 *
 * this LFUFast clear the frequency of an obj_id after evicting from cache
 */

#include "../include/libCacheSim/evictionAlgo/LFUFast.h"
#include "../dataStructure/hashtable/hashtable.h"


#ifdef __cplusplus
extern "C" {
#endif

static void free_list_node(void *list_node) {
  my_free(sizeof(freq_node_t), list_node);
}

static int _verify(cache_t *cache) {
  LFUFast_params_t *LFUFastDA_params = (LFUFast_params_t *) (cache->eviction_params);
  cache_obj_t *cache_obj, *prev_obj;
  /* update min freq */
  for (uint64_t freq = 1; freq <= LFUFastDA_params->max_freq; freq++) {
    freq_node_t *freq_node = g_hash_table_lookup(LFUFastDA_params->freq_map, GSIZE_TO_POINTER(freq));
    if (freq_node != NULL) {
      uint32_t n_obj = 0;
      cache_obj = freq_node->first_obj;
      prev_obj = NULL;
      while (cache_obj != NULL) {
        n_obj ++;
        DEBUG_ASSERT(cache_obj->common.freq == freq);
        DEBUG_ASSERT(cache_obj->common.list_prev == prev_obj);
        prev_obj = cache_obj;
        cache_obj = cache_obj->common.list_next;
      }
      DEBUG_ASSERT(freq_node->n_obj == n_obj);
    }
  }
  return 0;
}

cache_t *LFUFast_init(common_cache_params_t ccache_params,
                      void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("LFUFast", ccache_params);
  cache->cache_init = LFUFast_init;
  cache->cache_free = LFUFast_free;
  cache->get = LFUFast_get;
  cache->check = LFUFast_check;
  cache->insert = LFUFast_insert;
  cache->evict = LFUFast_evict;
  cache->remove = LFUFast_remove;

  LFUFast_params_t *params = my_malloc_n(LFUFast_params_t, 1);
  cache->eviction_params = params;

  params->min_freq = 1;
  params->max_freq = 1;

  freq_node_t *freq_node = my_malloc_n(freq_node_t, 1);
  params->freq_one_node = freq_node;
  freq_node->freq = 1;
  freq_node->n_obj = 0;
  freq_node->first_obj = NULL;
  freq_node->last_obj = NULL;

  params->freq_map = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) free_list_node);
  g_hash_table_insert(params->freq_map, GSIZE_TO_POINTER(1), freq_node);

  return cache;
}

void LFUFast_free(cache_t *cache) {
  LFUFast_params_t *params = (LFUFast_params_t *) (cache->eviction_params);
  g_hash_table_destroy(params->freq_map);
  my_free(sizeof(LFUFast_params_t), params);
  cache_struct_free(cache);
}

cache_ck_res_e LFUFast_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *cache_obj;
  cache_ck_res_e ret = cache_check_base(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    LFUFast_params_t *params = (LFUFast_params_t *) (cache->eviction_params);
    /* freq incr and move to next freq node */
    cache_obj->common.freq += 1;
    if (params->max_freq < cache_obj->common.freq) {
      params->max_freq = cache_obj->common.freq;
    }

    freq_node_t *new_node = g_hash_table_lookup(
        params->freq_map, GSIZE_TO_POINTER(cache_obj->common.freq));
    if (new_node == NULL) {
      new_node = my_malloc_n(freq_node_t, 1);
      new_node->freq = cache_obj->common.freq;
      new_node->n_obj = 1;
      new_node->first_obj = cache_obj;
      /* to make sure when link the tail to cache_obj, it will not become a circle */
      new_node->last_obj = NULL;
      g_hash_table_insert(params->freq_map, GSIZE_TO_POINTER(cache_obj->common.freq), new_node);
//      DEBUG("allocate new %d %d %p %p\n", new_node->freq, new_node->n_obj, new_node->first_obj, new_node->last_obj);
    } else {
      DEBUG_ASSERT(new_node->freq == cache_obj->common.freq);
      new_node->n_obj += 1;
    }

    freq_node_t *old_node = g_hash_table_lookup(
        params->freq_map, GSIZE_TO_POINTER(cache_obj->common.freq - 1));
    DEBUG_ASSERT(old_node != NULL);
    DEBUG_ASSERT(old_node->freq == cache_obj->common.freq - 1);
    DEBUG_ASSERT(old_node->n_obj > 0);
    old_node->n_obj -= 1;
    remove_obj_from_list(&old_node->first_obj, &old_node->last_obj, cache_obj);

    cache_obj->common.list_prev = new_node->last_obj;
    cache_obj->common.list_next = NULL;

    /* add to tail of the list */
    if (new_node->last_obj != NULL) {
      new_node->last_obj->common.list_next = cache_obj;
    }
    new_node->last_obj = cache_obj;
    if (new_node->first_obj == NULL)
      new_node->first_obj = cache_obj;
  }
  return ret;
}

cache_ck_res_e LFUFast_get(cache_t *cache, request_t *req) {
//  DEBUG_ASSERT(_verify(cache) == 0);
  return cache_get_base(cache, req);
}

void LFUFast_insert(cache_t *cache, request_t *req) {
  LFUFast_params_t *params = (LFUFast_params_t *) (cache->eviction_params);
  params->min_freq = 1;
  freq_node_t *freq_one_node = params->freq_one_node;

  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  cache_obj->common.freq = 1;
  cache_obj->common.list_prev = freq_one_node->last_obj;
  freq_one_node->n_obj += 1;

  if (freq_one_node->last_obj != NULL) {
    freq_one_node->last_obj->common.list_next = cache_obj;
  } else {
    DEBUG_ASSERT(freq_one_node->first_obj == NULL);
    freq_one_node->first_obj = cache_obj;
  }
  freq_one_node->last_obj = cache_obj;
  DEBUG_ASSERT(freq_one_node->first_obj != NULL);
//  if (freq_one_node->first_obj == NULL)
//    freq_one_node->first_obj = cache_obj;
}

void LFUFast_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  LFUFast_params_t *params = (LFUFast_params_t *) (cache->eviction_params);

  freq_node_t *min_freq_node = g_hash_table_lookup(
      params->freq_map, GSIZE_TO_POINTER(params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->first_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);
  min_freq_node->n_obj--;

  cache_obj_t *obj_to_evict = min_freq_node->first_obj;
  if (evicted_obj != NULL)
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));

  if (obj_to_evict->common.list_next == NULL) {
    /* the only obj of curr freq */
    DEBUG_ASSERT(min_freq_node->last_obj == obj_to_evict);
    DEBUG_ASSERT(min_freq_node->n_obj == 0);
    min_freq_node->first_obj = NULL;
    min_freq_node->last_obj = NULL;

    /* update min freq */
    for (uint64_t freq = params->min_freq + 1; freq <= params->max_freq; freq++) {
      if (g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(freq)) != NULL) {
        params->min_freq = freq;
        break;
      }
    }

  } else {
    min_freq_node->first_obj = obj_to_evict->common.list_next;
    obj_to_evict->common.list_next->common.list_prev = NULL;
  }

  cache_remove_obj_base(cache, obj_to_evict);
}

void LFUFast_remove(cache_t *cache, obj_id_t obj_id) {
  LFUFast_params_t *params = (LFUFast_params_t *) (cache->eviction_params);
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    WARNING("obj to remove is not in the cache\n");
    return;
  }

  freq_node_t *freq_node = g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(obj->common.freq));
  DEBUG_ASSERT(freq_node->freq == obj->common.freq);
  DEBUG_ASSERT(freq_node->n_obj > 0);

  freq_node->n_obj--;
  remove_obj_from_list(&freq_node->first_obj, &freq_node->last_obj, obj);

  cache_remove_obj_base(cache, obj);
}


#ifdef __cplusplus
}
#endif
