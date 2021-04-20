//
//  LFUDA.c
//  libCacheSim
//
//  Created by Juncheng on 9/28/20.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifdef __cplusplus
extern "C" {
#endif

#include "../include/libCacheSim/evictionAlgo/LFUDA.h"
#include "../dataStructure/hashtable/hashtable.h"

static void free_list_node(void *list_node) {
  my_free(sizeof(freq_node_t), list_node);
}

static int _verify(cache_t *cache) {
  LFUDA_params_t *LFUDA_params = (LFUDA_params_t *) (cache->cache_params);
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

cache_t *LFUDA_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("LFUDA", ccache_params);

  LFUDA_params_t *LFUDA_params = my_malloc_n(LFUDA_params_t, 1);
  cache->cache_params = LFUDA_params;

  LFUDA_params->min_freq = 1;
  LFUDA_params->max_freq = 1;

  freq_node_t *freq_node = my_malloc_n(freq_node_t, 1);
  freq_node->freq = 1;
  freq_node->n_obj = 0;
  freq_node->first_obj = NULL;
  freq_node->last_obj = NULL;

  LFUDA_params->freq_map = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) free_list_node);
  g_hash_table_insert(LFUDA_params->freq_map, GSIZE_TO_POINTER(1), freq_node);

  return cache;
}

void LFUDA_free(cache_t *cache) {
  LFUDA_params_t *LFUDA_params = (LFUDA_params_t *) (cache->cache_params);
  g_hash_table_destroy(LFUDA_params->freq_map);
  cache_struct_free(cache);
}

cache_ck_res_e LFUDA_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *cache_obj;
  cache_ck_res_e ret = cache_check(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    LFUDA_params_t *LFUDA_params = (LFUDA_params_t *) (cache->cache_params);
    /* freq incr and move to next freq node */
    cache_obj->freq += LFUDA_params->min_freq;
    if (LFUDA_params->max_freq < cache_obj->freq) {
      LFUDA_params->max_freq = cache_obj->freq;
    }

    freq_node_t *new_node = g_hash_table_lookup(
        LFUDA_params->freq_map, GSIZE_TO_POINTER(cache_obj->freq));
    if (new_node == NULL) {
      new_node = my_malloc_n(freq_node_t, 1);
      new_node->freq = cache_obj->freq;
      new_node->n_obj = 1;
      new_node->first_obj = cache_obj;
      new_node->last_obj = NULL;
      g_hash_table_insert(LFUDA_params->freq_map, GSIZE_TO_POINTER(cache_obj->freq), new_node);
    } else {
      DEBUG_ASSERT(new_node->freq == cache_obj->freq);
      new_node->n_obj += 1;
    }

    freq_node_t *old_node = g_hash_table_lookup(
        LFUDA_params->freq_map, GSIZE_TO_POINTER(cache_obj->freq - LFUDA_params->min_freq));
    DEBUG_ASSERT(old_node != NULL);
    DEBUG_ASSERT(old_node->freq == cache_obj->freq - LFUDA_params->min_freq);
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

cache_ck_res_e LFUDA_get(cache_t *cache, request_t *req) {
//  DEBUG_ASSERT(_verify(cache) == 0);
//  static __thread int min_freq = 1;
//  if (((LFUDA_params_t *) (cache->cache_params))->min_freq != min_freq) {
//    printf("min freq %lu\n", ((LFUDA_params_t *) (cache->cache_params))->min_freq);
//    min_freq = ((LFUDA_params_t *) (cache->cache_params))->min_freq;
//  }
  return cache_get(cache, req);
}

void LFUDA_remove(cache_t *cache, obj_id_t obj_id) {
  LFUDA_params_t *LFUDA_params = (LFUDA_params_t *) (cache->cache_params);
  cache_obj_t *cache_obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (cache_obj == NULL) {
    WARNING("obj to remove is not in the cache\n");
    return;
  }

  freq_node_t *freq_node = g_hash_table_lookup(LFUDA_params->freq_map, GSIZE_TO_POINTER(cache_obj->freq));
  DEBUG_ASSERT(freq_node->freq == cache_obj->freq);
  DEBUG_ASSERT(freq_node->n_obj > 0);

  freq_node->n_obj--;
  remove_obj_from_list(&freq_node->first_obj, &freq_node->last_obj, cache_obj);
  cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_overhead);

  hashtable_delete(cache->hashtable, cache_obj);
}

void LFUDA_insert(cache_t *cache, request_t *req) {
  LFUDA_params_t *LFUDA_params = (LFUDA_params_t *) (cache->cache_params);

#ifdef SUPPORT_TTL
  if (cache->default_ttl != 0 && req->ttl == 0) {
    req->ttl = cache->default_ttl;
  }
#endif
  cache->occupied_size += req->obj_size + cache->per_obj_overhead;
  cache_obj_t *cache_obj = hashtable_insert(cache->hashtable, req);
  cache_obj->freq = LFUDA_params->min_freq + 1;
  freq_node_t *new_node = g_hash_table_lookup(LFUDA_params->freq_map, GSIZE_TO_POINTER(cache_obj->freq));
  if (new_node == NULL) {
    new_node = my_malloc_n(freq_node_t, 1);
    new_node->freq = cache_obj->freq;
    new_node->n_obj = 1;
    new_node->first_obj = cache_obj;
    new_node->last_obj = NULL;
    g_hash_table_insert(LFUDA_params->freq_map, GSIZE_TO_POINTER(cache_obj->freq), new_node);
  } else {
    DEBUG_ASSERT(new_node->freq == cache_obj->freq);
    new_node->n_obj += 1;
  }

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

void LFUDA_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  LFUDA_params_t *LFUDA_params = (LFUDA_params_t *) (cache->cache_params);

  freq_node_t *min_freq_node = g_hash_table_lookup(
      LFUDA_params->freq_map, GSIZE_TO_POINTER(LFUDA_params->min_freq));
  if (min_freq_node == NULL || min_freq_node->first_obj == NULL)
    min_freq_node = g_hash_table_lookup(LFUDA_params->freq_map, GSIZE_TO_POINTER(LFUDA_params->min_freq + 1));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->first_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);
  min_freq_node->n_obj--;

  LFUDA_params->min_freq = min_freq_node->freq;
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
    for (uint64_t freq = LFUDA_params->min_freq + 1; freq <= LFUDA_params->max_freq; freq++) {
      if (g_hash_table_lookup(LFUDA_params->freq_map, GSIZE_TO_POINTER(freq)) != NULL) {
        LFUDA_params->min_freq = freq;
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
