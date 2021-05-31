//
//  LFU.c
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

/* this module uses linkedlist to order requests,
 * which is O(1) at each request, which should be preferred in most cases
 * the drawback of this implementation is the memory usage, because two pointers
 * are associated with each obj_id
 *
 * this LFU clear the frequency of an obj_id after evicting from cache
 * this LFU is LFU_LFU, which choose LFU when more than one items have
 * the same smallest freq
 */

#include "../include/libCacheSim/evictionAlgo/LFU.h"
#include "../dataStructure/hashtable/hashtable.h"


#ifdef __cplusplus
extern "C" {
#endif

static void free_list_node(void *list_node) {
  my_free(sizeof(freq_node_t), list_node);
}

static int _verify(cache_t *cache) {
  LFU_params_t *LFUDA_params = (LFU_params_t *) (cache->eviction_params);
  cache_obj_t *cache_obj, *prev_obj;
  /* update min freq */
  for (uint64_t freq = 1; freq <= LFUDA_params->max_freq; freq++) {
    freq_node_t *freq_node = g_hash_table_lookup(LFUDA_params->freq_map, GSIZE_TO_POINTER(freq));
    if (freq_node != NULL) {
      uint32_t n_obj = 0;
      cache_obj = freq_node->first_obj;
      prev_obj = NULL;
      while (cache_obj != NULL) {
        n_obj ++;
        DEBUG_ASSERT(cache_obj->freq == freq);
        DEBUG_ASSERT(cache_obj->list_prev == prev_obj);
        prev_obj = cache_obj;
        cache_obj = cache_obj->list_next;
      }
      DEBUG_ASSERT(freq_node->n_obj == n_obj);
    }
  }
  return 0;
}

cache_t *LFU_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("LFU", ccache_params);
  cache->cache_init = LFU_init;
  cache->cache_free = LFU_free;
  cache->get = LFU_get;
  cache->check = LFU_check;
  cache->insert = LFU_insert;
  cache->evict = LFU_evict;
  cache->remove = LFU_remove;

  LFU_params_t *LFU_params = my_malloc_n(LFU_params_t, 1);
  cache->eviction_params = LFU_params;

  LFU_params->min_freq = 1;
  LFU_params->max_freq = 1;

  freq_node_t *freq_node = my_malloc_n(freq_node_t, 1);
  LFU_params->freq_one_node = freq_node;
  freq_node->freq = 1;
  freq_node->n_obj = 0;
  freq_node->first_obj = NULL;
  freq_node->last_obj = NULL;

  LFU_params->freq_map = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) free_list_node);
  g_hash_table_insert(LFU_params->freq_map, GSIZE_TO_POINTER(1), freq_node);

  return cache;
}

void LFU_free(cache_t *cache) {
  LFU_params_t *LFU_params = (LFU_params_t *) (cache->eviction_params);
  g_hash_table_destroy(LFU_params->freq_map);
  cache_struct_free(cache);
}

cache_ck_res_e LFU_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *cache_obj;
  cache_ck_res_e ret = cache_check_base(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    LFU_params_t *LFU_params = (LFU_params_t *) (cache->eviction_params);
    /* freq incr and move to next freq node */
    cache_obj->freq += 1;
    if (LFU_params->max_freq < cache_obj->freq) {
      LFU_params->max_freq = cache_obj->freq;
    }

    freq_node_t *new_node = g_hash_table_lookup(
        LFU_params->freq_map, GSIZE_TO_POINTER(cache_obj->freq));
    if (new_node == NULL) {
      new_node = my_malloc_n(freq_node_t, 1);
      new_node->freq = cache_obj->freq;
      new_node->n_obj = 1;
      new_node->first_obj = cache_obj;
      new_node->last_obj = NULL;
      g_hash_table_insert(LFU_params->freq_map, GSIZE_TO_POINTER(cache_obj->freq), new_node);
//      DEBUG("allocate new %d %d %p %p\n", new_node->freq, new_node->n_obj, new_node->first_obj, new_node->last_obj);
    } else {
      DEBUG_ASSERT(new_node->freq == cache_obj->freq);
      new_node->n_obj += 1;
    }

    freq_node_t *old_node = g_hash_table_lookup(
        LFU_params->freq_map, GSIZE_TO_POINTER(cache_obj->freq - 1));
    DEBUG_ASSERT(old_node != NULL);
    DEBUG_ASSERT(old_node->freq == cache_obj->freq - 1);
    DEBUG_ASSERT(old_node->n_obj > 0);
    old_node->n_obj -= 1;

    remove_obj_from_list(&old_node->first_obj, &old_node->last_obj, cache_obj);

    cache_obj->list_prev = new_node->last_obj;
    cache_obj->list_next = NULL;

    /* add to tail of the list */
    if (new_node->last_obj != NULL) {
      new_node->last_obj->list_next = cache_obj;
    }
    new_node->last_obj = cache_obj;
    if (new_node->first_obj == NULL)
      new_node->first_obj = cache_obj;
  }
  return ret;
}

cache_ck_res_e LFU_get(cache_t *cache, request_t *req) {
//  DEBUG_ASSERT(_verify(cache) == 0);
  return cache_get_base(cache, req);
}

void LFU_remove(cache_t *cache, obj_id_t obj_id) {
  LFU_params_t *LFU_params = (LFU_params_t *) (cache->eviction_params);
  cache_obj_t *cache_obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (cache_obj == NULL) {
    WARNING("obj to remove is not in the cache\n");
    return;
  }

  freq_node_t *freq_node = g_hash_table_lookup(LFU_params->freq_map, GSIZE_TO_POINTER(cache_obj->freq));
  DEBUG_ASSERT(freq_node->freq == cache_obj->freq);
  DEBUG_ASSERT(freq_node->n_obj > 0);

  freq_node->n_obj--;
  remove_obj_from_list(&freq_node->first_obj, &freq_node->last_obj, cache_obj);
  cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_overhead);
  cache->n_obj -= 1;

  hashtable_delete(cache->hashtable, cache_obj);
}

void LFU_insert(cache_t *cache, request_t *req) {
  LFU_params_t *LFU_params = (LFU_params_t *) (cache->eviction_params);

#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  if (cache->default_ttl != 0 && req->ttl == 0) {
    req->ttl = cache->default_ttl;
  }
#endif
  LFU_params->min_freq = 1;
  cache->occupied_size += req->obj_size + cache->per_obj_overhead;
  cache_obj_t *cache_obj = hashtable_insert(cache->hashtable, req);
  cache_obj->freq = 1;
  freq_node_t *freq_one_node = LFU_params->freq_one_node;
  cache_obj->list_prev = freq_one_node->last_obj;
  freq_one_node->n_obj += 1;

  if (freq_one_node->last_obj != NULL) {
    freq_one_node->last_obj->list_next = cache_obj;
  } else {
    DEBUG_ASSERT(freq_one_node->first_obj == NULL);
    freq_one_node->first_obj = cache_obj;
  }
  freq_one_node->last_obj = cache_obj;
  if (freq_one_node->first_obj == NULL)
    freq_one_node->first_obj = cache_obj;
}

void LFU_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  LFU_params_t *LFU_params = (LFU_params_t *) (cache->eviction_params);

  freq_node_t *min_freq_node = g_hash_table_lookup(
      LFU_params->freq_map, GSIZE_TO_POINTER(LFU_params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->first_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);
  min_freq_node->n_obj--;

  cache_obj_t *obj_to_evict = min_freq_node->first_obj;
  if (evicted_obj != NULL)
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));

  cache->occupied_size -= (obj_to_evict->obj_size + cache->per_obj_overhead);

  if (obj_to_evict->list_next == NULL) {
    /* the only obj of curr freq */
    DEBUG_ASSERT(min_freq_node->last_obj == obj_to_evict);
    DEBUG_ASSERT(min_freq_node->n_obj == 0);
    min_freq_node->first_obj = NULL;
    min_freq_node->last_obj = NULL;

    /* update min freq */
    for (uint64_t freq = LFU_params->min_freq + 1; freq <= LFU_params->max_freq; freq++) {
      if (g_hash_table_lookup(LFU_params->freq_map, GSIZE_TO_POINTER(freq)) != NULL) {
        LFU_params->min_freq = freq;
        break;
      }
    }

  } else {
    min_freq_node->first_obj = obj_to_evict->list_next;
    obj_to_evict->list_next->list_prev = NULL;
  }

  hashtable_delete(cache->hashtable, obj_to_evict);
}

#ifdef __cplusplus
}
#endif
