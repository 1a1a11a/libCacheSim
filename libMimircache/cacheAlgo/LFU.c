//
//  LFU.c
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

/* this module uses priority queue to order cached content,
 * which is O(logN) at each request,
 * but this approach saves some memory compared to the other approach,
 * which uses a hashmap and linkedlist and gives O(1) at each request
 *
 * this LFU clear the frequency of an obj_id after evicting from cacheAlgo
 * when there are more than one items with the smallest freq, the behavior is
 * LRU, but can be tuned to MRU, Random or unstable-pq-decided
 */


#ifdef __cplusplus
extern "C" {
#endif

#include "LFU.h"
#include "../../include/mimircache/cacheOp.h"


/********************* priority queue structs and def ***********************/
static inline int cmp_pri(pqueue_pri_t next, pqueue_pri_t curr) {
  /* the one with smallest priority is poped out first */
  if (next.pri1 == curr.pri1)
    return (next.pri2 > curr.pri2); // LRU
  else
    return (next.pri1 > curr.pri1);
}

static inline pqueue_pri_t get_pri(void *a) { return ((pq_node_t *) a)->pri; }

static inline void set_pri(void *a, pqueue_pri_t pri) {
  ((pq_node_t *) a)->pri = pri;
}

static inline size_t get_pos(void *a) { return ((pq_node_t *) a)->pos; }

static inline void set_pos(void *a, size_t pos) { ((pq_node_t *) a)->pos = pos; }

/********************************** LFU ************************************/
void _LFU_insert(cache_t *LFU, request_t *req) {
  LFU_params_t *LFU_params = (LFU_params_t *) (LFU->cache_params);

  pq_node_t *node = g_new(pq_node_t, 1);
  gpointer key = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    key = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }
  node->obj_id_type = req->obj_id_type;
  node->obj_id = (gpointer) key;

  // frequency not cleared after eviction
  //    gpointer gp = g_hash_table_lookup(LFU_params->hashtable, req->obj_id_ptr);
  //    node->pri = GPOINTER_TO_UINT(gp)+1;

  /* node priority is set to one for first time,
   * frequency cleared after eviction
   */
  node->pri.pri1 = 1;
  node->pri.pri2 = LFU->core->req_cnt;

  pqueue_insert(LFU_params->pq, (void *) node);
  g_hash_table_insert(LFU_params->hashtable, (gpointer) key, (gpointer) node);
}

gboolean LFU_check(cache_t *cache, request_t *req) {
  return g_hash_table_contains(
    ((LFU_params_t *) (cache->cache_params))->hashtable,
    (gconstpointer) (req->obj_id_ptr));
}

void _LFU_update(cache_t *cache, request_t *req) {
  LFU_params_t *LFU_params = (LFU_params_t *) (cache->cache_params);
  pq_node_t *node = (pq_node_t *) g_hash_table_lookup(
    LFU_params->hashtable, (gconstpointer) (req->obj_id_ptr));
  pqueue_pri_t pri;
  pri.pri1 = node->pri.pri1 + 1;
  pri.pri2 = cache->core->req_cnt;
  pqueue_change_priority(LFU_params->pq, pri, (void *) node);
}

void _LFU_evict(cache_t *cache, request_t *req) {
  LFU_params_t *LFU_params = (LFU_params_t *) (cache->cache_params);
  pq_node_t *node = (pq_node_t *) pqueue_pop(LFU_params->pq);
  g_hash_table_remove(LFU_params->hashtable, (gconstpointer) (node->obj_id));
}

gpointer _LFU_evict_with_return(cache_t *cache, request_t *req) {
  LFU_params_t *LFU_params = (LFU_params_t *) (cache->cache_params);

  pq_node_t *node = (pq_node_t *) pqueue_pop(LFU_params->pq);
  gpointer evicted_key = node->obj_id;
  if (req->obj_id_type == OBJ_ID_STR) {
    evicted_key = (gpointer) g_strdup((gchar *) node->obj_id);
  }
  g_hash_table_remove(LFU_params->hashtable, (gconstpointer) (node->obj_id));
  return evicted_key;
}

gboolean LFU_add(cache_t *cache, request_t *req) {
  LFU_params_t *LFU_params = (LFU_params_t *) (cache->cache_params);
  gboolean retval;
  if (LFU_check(cache, req)) {
    _LFU_update(cache, req);
    retval = TRUE;
  } else {
    _LFU_insert(cache, req);
    if ((long) g_hash_table_size(LFU_params->hashtable) > cache->core->size)
      _LFU_evict(cache, req);
    retval = FALSE;
  }
  cache->core->req_cnt += 1;
  return retval;
}

gboolean LFU_add_only(cache_t *cache, request_t *req) {
  return LFU_add(cache, req);
}

gboolean LFU_add_with_size(cache_t *cache, request_t *req) {
  gint64 original_lbn = *(gint64 *) (req->obj_id_ptr);
  gboolean ret_val;

  ret_val = LFU_add(cache, req);

  *(gint64 *) (req->obj_id_ptr) = original_lbn;
  cache->core->req_cnt += 1;
  return ret_val;
}

void LFU_destroy(cache_t *cache) {
  LFU_params_t *LFU_params = (LFU_params_t *) (cache->cache_params);

  g_hash_table_destroy(LFU_params->hashtable);
  pqueue_free(LFU_params->pq);
  cache_struct_free(cache);
}

void LFU_destroy_unique(cache_t *cache) {
  /* the difference between destroy_cloned_cache and destroy
   is that the former one only free the resources that are
   unique to the cacheAlgo, freeing these resources won't affect
   other caches copied from original cacheAlgo
   in LFU, next_access should not be freed in destroy_cloned_cache,
   because it is shared between different caches copied from the original one.
   */
  LFU_destroy(cache);
}

cache_t *LFU_init(guint64 size, obj_id_type_t obj_id_type, void *params) {
  cache_t *cache = cache_struct_init("LFU", size, obj_id_type);
  LFU_params_t *LFU_params = g_new0(LFU_params_t, 1);
  cache->cache_params = (void *) LFU_params;

//  cacheAlgo->core->add_only = LFU_add;


  if (obj_id_type == OBJ_ID_NUM) {
    LFU_params->hashtable = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
                                                  g_free);
  } else if (obj_id_type == OBJ_ID_STR) {
    LFU_params->hashtable = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free,
                                                  g_free);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }

  LFU_params->pq =
    pqueue_init(size, cmp_pri, get_pri, set_pri, get_pos, set_pos);

  return cache;
}

guint64 LFU_get_size(cache_t *cache) {
  LFU_params_t *LFU_params = (LFU_params_t *) (cache->cache_params);
  return (guint64) g_hash_table_size(LFU_params->hashtable);
}

void LFU_remove_obj(cache_t *cache, void *data_to_remove) {
  LFU_params_t *LFU_params = (LFU_params_t *) (cache->cache_params);
  pq_node_t *node = (pq_node_t *) g_hash_table_lookup(
    LFU_params->hashtable, (gconstpointer) data_to_remove);
  pqueue_remove(LFU_params->pq, (void *) node);
  g_hash_table_remove(LFU_params->hashtable, (gconstpointer) data_to_remove);
}

#ifdef __cplusplus
}
#endif
