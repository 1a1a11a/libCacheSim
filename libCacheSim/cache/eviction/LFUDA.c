//
//  LFUDA.c
//  libCacheSim
//
//  Created by Juncheng on 9/28/20.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include <glib.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/LFUDA.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LFUDA_params {
  GHashTable *freq_map;
  freq_node_t *freq_one_node;
  int64_t min_freq;
  int64_t max_freq;
} LFUDA_params_t;

static void free_list_node(void *list_node); 
static inline freq_node_t *get_min_freq_node(LFUDA_params_t *params);
static inline void update_min_freq(LFUDA_params_t *params);

// ****************** end user facing functions *******************
cache_t *LFUDA_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LFUDA", ccache_params);
  cache->cache_init = LFUDA_init;
  cache->cache_free = LFUDA_free;
  cache->get = LFUDA_get;
  cache->check = LFUDA_check;
  cache->insert = LFUDA_insert;
  cache->evict = LFUDA_evict;
  cache->remove = LFUDA_remove;
  cache->to_evict = LFUDA_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  if (cache_specific_params != NULL) {
    ERROR("LFUDA does not support any parameters, but got %s\n",
          cache_specific_params);
    abort();
  }

  LFUDA_params_t *params = my_malloc_n(LFUDA_params_t, 1);
  cache->eviction_params = params;

  params->min_freq = 0;
  params->max_freq = 0;

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

void LFUDA_free(cache_t *cache) {
  LFUDA_params_t *params = (LFUDA_params_t *)(cache->eviction_params);
  g_hash_table_destroy(params->freq_map);
  cache_struct_free(cache);
}

bool LFUDA_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

// *********** developer facing APIs (used by cache developer) ***********
/**
 * @brief check whether an object is in the cache, update the meatadata
 * if update_cache is true
 */
bool LFUDA_check(cache_t *cache, const request_t *req,
                 const bool update_cache) {
  cache_obj_t *cache_obj;
  LFUDA_params_t *params = (LFUDA_params_t *)(cache->eviction_params);
  bool ret = cache_check_base(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    /* freq incr and move to next freq node */
    cache_obj->lfu.freq += params->min_freq;
    params->max_freq = params->max_freq < cache_obj->lfu.freq
                           ? cache_obj->lfu.freq
                           : params->max_freq;

    gpointer old_key = GSIZE_TO_POINTER(cache_obj->lfu.freq - params->min_freq);
    freq_node_t *old_node = g_hash_table_lookup(params->freq_map, old_key);
    DEBUG_ASSERT(old_node != NULL);
    DEBUG_ASSERT(old_node->freq == cache_obj->lfu.freq - params->min_freq);
    DEBUG_ASSERT(old_node->n_obj > 0);
    old_node->n_obj--;
    remove_obj_from_list(&old_node->first_obj, &old_node->last_obj,
                           cache_obj);

    // if the old freq_node has one object and is the min_freq_node, after
    // removing this object, the freq_node will have no object,
    // then we should update min_freq to the new freq
    if (params->min_freq == old_node->freq && old_node->n_obj == 0) {
      /* update min freq */
      update_min_freq(params);
    }

    gpointer new_key = GSIZE_TO_POINTER(cache_obj->lfu.freq);
    freq_node_t *new_node = g_hash_table_lookup(params->freq_map, new_key);
    if (new_node == NULL) {
      new_node = my_malloc_n(freq_node_t, 1);
      memset(new_node, 0, sizeof(freq_node_t));
      new_node->freq = cache_obj->lfu.freq;
      g_hash_table_insert(params->freq_map, new_key, new_node);
      VVVERBOSE("allocate new %d %d %p %p\n", new_node->freq, new_node->n_obj,
                new_node->first_obj, new_node->last_obj);
    } else {
      // it could be new_node is empty
      DEBUG_ASSERT(new_node->freq == cache_obj->lfu.freq);
    }

    append_obj_to_tail(&new_node->first_obj, &new_node->last_obj, cache_obj);
    new_node->n_obj += 1;
  }

  return ret;
}

/**
 * @brief insert an object into LFUDA cache, this assumes that
 * the cache has enough space for this object, eviction should
 * perform before calling this function
 */
cache_obj_t *LFUDA_insert(cache_t *cache, const request_t *req) {
  LFUDA_params_t *params = (LFUDA_params_t *)(cache->eviction_params);
  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  cache_obj->lfu.freq = params->min_freq + 1;

  gpointer key = GSIZE_TO_POINTER(cache_obj->lfu.freq);
  freq_node_t *new_node = g_hash_table_lookup(params->freq_map, key);
  if (new_node == NULL) {
    new_node = my_malloc_n(freq_node_t, 1);
    memset(new_node, 0, sizeof(freq_node_t));
    new_node->freq = cache_obj->lfu.freq;
    g_hash_table_insert(params->freq_map, key, new_node);
  } else {
    DEBUG_ASSERT(new_node->freq == cache_obj->lfu.freq);
  }

  append_obj_to_tail(&new_node->first_obj, &new_node->last_obj, cache_obj);
  new_node->n_obj += 1;

  return cache_obj;
}

cache_obj_t *LFUDA_to_evict(cache_t *cache) {
  LFUDA_params_t *params = (LFUDA_params_t *)(cache->eviction_params);
  freq_node_t *min_freq_node = get_min_freq_node(params);
  return min_freq_node->first_obj;
}

void LFUDA_evict(cache_t *cache, const request_t *req,
                 cache_obj_t *evicted_obj) {
  LFUDA_params_t *params = (LFUDA_params_t *)(cache->eviction_params);

  freq_node_t *min_freq_node = get_min_freq_node(params);
  cache_obj_t *obj_to_evict = min_freq_node->first_obj;
  if (evicted_obj != NULL) {
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }

  params->min_freq = min_freq_node->freq;
  min_freq_node->n_obj--;
  remove_obj_from_list(&min_freq_node->first_obj, &min_freq_node->last_obj,
                       obj_to_evict);

  cache_evict_base(cache, obj_to_evict, true);

  if (min_freq_node->n_obj == 0) {
    /* the only obj of curr freq */
    DEBUG_ASSERT(min_freq_node->first_obj == NULL);
    DEBUG_ASSERT(min_freq_node->last_obj == NULL);

    /* update min freq */
    update_min_freq(params);
  }
}

void LFUDA_remove_obj(cache_t *cache, cache_obj_t *obj) {
  assert(obj != NULL);
  LFUDA_params_t *params = (LFUDA_params_t *)(cache->eviction_params);

  gpointer key = GSIZE_TO_POINTER(obj->lfu.freq);
  freq_node_t *freq_node = g_hash_table_lookup(params->freq_map, key);
  DEBUG_ASSERT(freq_node->freq == obj->lfu.freq);
  DEBUG_ASSERT(freq_node->n_obj > 0);

  freq_node->n_obj--;
  remove_obj_from_list(&freq_node->first_obj, &freq_node->last_obj, obj);

  cache_remove_obj_base(cache, obj, true);

  if (freq_node->freq == params->min_freq && freq_node->n_obj == 0) {
    DEBUG_ASSERT(freq_node->first_obj == NULL);
    DEBUG_ASSERT(freq_node->last_obj == NULL);

    /* update min freq */
    update_min_freq(params);
  }
}

bool LFUDA_remove(cache_t *cache, obj_id_t obj_id) {
  LFUDA_params_t *params = (LFUDA_params_t *)(cache->eviction_params);
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  LFUDA_remove_obj(cache, obj);

  return true;
}

// ****************** internal functions *******************
static freq_node_t *get_min_freq_node(LFUDA_params_t *params) {
  freq_node_t *min_freq_node = NULL;
  if (params->min_freq == 0 || params->min_freq == 1) {
    min_freq_node = params->freq_one_node;
  } else {
    DEBUG_ASSERT(params->min_freq > 1);
    min_freq_node = g_hash_table_lookup(params->freq_map,
                                        GSIZE_TO_POINTER(params->min_freq));
  }

  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->first_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);

  return min_freq_node;
}

static void update_min_freq(LFUDA_params_t *params) {
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

static void free_list_node(void *list_node) {
  my_free(sizeof(freq_node_t), list_node);
}

// ****************** internal debug use functions *******************
static int _verify(cache_t *cache) {
  LFUDA_params_t *LFUDA_params = (LFUDA_params_t *)(cache->eviction_params);
  cache_obj_t *cache_obj, *prev_obj;
  /* update min freq */
  for (uint64_t freq = 1; freq <= LFUDA_params->max_freq; freq++) {
    freq_node_t *freq_node =
        g_hash_table_lookup(LFUDA_params->freq_map, GSIZE_TO_POINTER(freq));
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
