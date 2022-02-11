#include "../include/libCacheSim/evictionAlgo/CR_LFU.h"
#include "../dataStructure/hashtable/hashtable.h"
// CR_LFU is used by Cacheus.

#ifdef __cplusplus
extern "C" {
#endif


static void free_list_node(void *list_node) {
  my_free(sizeof(freq_node_t), list_node);
}

static int _verify(cache_t *cache) {
  CR_LFU_params_t *CR_LFUDA_params = (CR_LFU_params_t *) (cache->eviction_params);
  cache_obj_t *cache_obj, *prev_obj;
  /* update min freq */
  for (uint64_t freq = 1; freq <= CR_LFUDA_params->max_freq; freq++) {
    freq_node_t *freq_node = g_hash_table_lookup(CR_LFUDA_params->freq_map, GSIZE_TO_POINTER(freq));
    if (freq_node != NULL) {
      uint32_t n_obj = 0;
      cache_obj = freq_node->first_obj;
      prev_obj = NULL;
      while (cache_obj != NULL) {
        n_obj ++;
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

cache_t *CR_LFU_init(common_cache_params_t ccache_params,
                      void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("CR_LFU", ccache_params);
  cache->cache_init = CR_LFU_init;
  cache->cache_free = CR_LFU_free;
  cache->get = CR_LFU_get;
  cache->check = CR_LFU_check;
  cache->insert = CR_LFU_insert;
  cache->evict = CR_LFU_evict;
  cache->remove = CR_LFU_remove;
  cache->to_evict = CR_LFU_to_evict;

  CR_LFU_params_t *params = my_malloc_n(CR_LFU_params_t, 1);
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

void CR_LFU_free(cache_t *cache) {
  CR_LFU_params_t *params = (CR_LFU_params_t *) (cache->eviction_params);
  g_hash_table_destroy(params->freq_map);
  my_free(sizeof(CR_LFU_params_t), params);
  cache_struct_free(cache);
}

cache_ck_res_e CR_LFU_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *cache_obj;
  cache_ck_res_e ret = cache_check_base(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    CR_LFU_params_t *params = (CR_LFU_params_t *) (cache->eviction_params);
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
      g_hash_table_insert(params->freq_map, GSIZE_TO_POINTER(cache_obj->lfu.freq), new_node);
      VVERBOSE("allocate new %d %d %p %p\n", new_node->freq, new_node->n_obj, new_node->first_obj, new_node->last_obj);
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

    // if the old freq_node only has one object, after removing this object, it will have no object
    // and if it is the min_freq, then we should update min_freq 
    if (params->min_freq == old_node->freq && old_node->n_obj == 0) {
      /* update min freq */
      uint64_t old_min_freq = params->min_freq;
      for (uint64_t freq = params->min_freq + 1; freq <= params->max_freq; freq++) {
        freq_node_t *node = g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(freq)); 
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

cache_ck_res_e CR_LFU_get(cache_t *cache, request_t *req) {
//  DEBUG_ASSERT(_verify(cache) == 0);
  return cache_get_base(cache, req);
}

void CR_LFU_insert(cache_t *cache, request_t *req) {
  CR_LFU_params_t *params = (CR_LFU_params_t *) (cache->eviction_params);
  params->min_freq = 1;
  freq_node_t *freq_one_node = params->freq_one_node;

  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  cache_obj->lfu.freq = 1;
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
}

cache_obj_t *CR_LFU_to_evict(cache_t *cache){

  CR_LFU_params_t *params = (CR_LFU_params_t *) (cache->eviction_params);

  freq_node_t *min_freq_node = g_hash_table_lookup(
      params->freq_map, GSIZE_TO_POINTER(params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->last_obj != NULL);
  DEBUG_ASSERT(min_freq_node->n_obj > 0);

  return min_freq_node->last_obj;
}

void CR_LFU_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  CR_LFU_params_t *params = (CR_LFU_params_t *) (cache->eviction_params);

  freq_node_t *min_freq_node = g_hash_table_lookup(
      params->freq_map, GSIZE_TO_POINTER(params->min_freq));
  DEBUG_ASSERT(min_freq_node != NULL);
  DEBUG_ASSERT(min_freq_node->last_obj != NULL);

  DEBUG_ASSERT(min_freq_node->n_obj > 0);
  min_freq_node->n_obj--;

  cache_obj_t *obj_to_evict = min_freq_node->last_obj;
  // cache_obj_t *obj_to_evict = min_freq_node->first_obj;

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
    for (uint64_t freq = params->min_freq + 1; freq <= params->max_freq; freq++) {
      freq_node_t *node = g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(freq)); 
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
}

void CR_LFU_remove(cache_t *cache, obj_id_t obj_id) {
  CR_LFU_params_t *params = (CR_LFU_params_t *) (cache->eviction_params);
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    raise(SIGINT);
    WARN("obj to remove is not in the cache\n");
    return;
  }

  freq_node_t *freq_node = g_hash_table_lookup(params->freq_map, GSIZE_TO_POINTER(obj->lfu.freq));
  DEBUG_ASSERT(freq_node->freq == obj->lfu.freq);
  DEBUG_ASSERT(freq_node->n_obj > 0);

  freq_node->n_obj--;
  remove_obj_from_list(&freq_node->first_obj, &freq_node->last_obj, obj);

  cache_remove_obj_base(cache, obj);
}


#ifdef __cplusplus
}
#endif
