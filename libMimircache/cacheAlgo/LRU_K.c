//
//  LRU_K.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//


/* need to add support for p and c obj_id_type of data
   need to control the size of ghost_hashtable

 */

/* some things do not look correct, need to check the implementation */


#ifdef __cplusplus
extern "C" {
#endif


#include "LRU_K.h"
#include "../../include/mimircache/cacheOp.h"


void gqueue_destroyer(gpointer data) {
  g_queue_free_full(data, g_free);
}

/********************* priority queue structs and def ***********************/
static int cmp_pri(pqueue_pri_t next, pqueue_pri_t curr) {
  /* the one with smallest priority is poped out first */
  return (next.pri1 > curr.pri1);
}

static pqueue_pri_t get_pri(void *a) { return ((pq_node_t *)a)->pri; }

static void set_pri(void *a, pqueue_pri_t pri) { ((pq_node_t *)a)->pri = pri; }

static size_t get_pos(void *a) { return ((pq_node_t *)a)->pos; }

static void set_pos(void *a, size_t pos) { ((pq_node_t *)a)->pos = pos; }

/********************************** LRU-K ***********************************/
/* Jason: try to reuse the key from ghost_hashtable for better memory efficiency
 */

void _LRU_K_insert(cache_t *LRU_K, request_t *req) {
  /** update request is done at checking element,
   * now insert request into cache_hashtable and pq
   *
   **/

  struct LRU_K_params *LRU_K_params =
      (struct LRU_K_params *)(LRU_K->cache_params);
  gpointer key = NULL;

  pq_node_t *node = g_new(pq_node_t, 1);
  node->obj_id_type = req->obj_id_type;

  GQueue *queue = NULL;
  g_hash_table_lookup_extended(LRU_K_params->ghost_hashtable,
                               (gconstpointer)(req->obj_id_ptr), &key,
                               (gpointer)&queue);

  pqueue_pri_t pri;
  if (queue->length < LRU_K_params->K) {
    pri.pri1 = INITIAL_TS;
  } else
    pri.pri1 = *(guint64 *)g_queue_peek_nth(queue, LRU_K_params->K - 1);

  node->obj_id = key;
  node->pri = pri;
  pqueue_insert(LRU_K_params->pq, (void *)node);
  g_hash_table_insert(LRU_K_params->cache_hashtable, (gpointer)key,
                      (gpointer)node);
}

gboolean LRU_K_check(cache_t *cache, request_t *req) {
  /** check whether request is in the cache_hashtable,
   * then update ghost_hashtable and pq accordingly,
   * if in ghost_hashtable, then update it,
   * else create a new entry in ghost_hashtable
   * the size of ghost_hashtable is maintained separately
   * if in cache_hashtable, update pq by updating priority value
   * if not in cache_hashtable, do nothing
   **/

  struct LRU_K_params *LRU_K_params =
      (struct LRU_K_params *)(cache->cache_params);
  GQueue *queue = g_hash_table_lookup(LRU_K_params->ghost_hashtable,
                                      (gconstpointer)(req->obj_id_ptr));
  if (queue == NULL) {

    /* need to insert the new element into ghost */
    gpointer key = req->obj_id_ptr;
    if (req->obj_id_type == OBJ_ID_STR) {
      key = (gpointer)g_strdup((gchar *)(req->obj_id_ptr));
    }

    queue = g_queue_new();
    g_hash_table_insert(LRU_K_params->ghost_hashtable, (gpointer)key,
                        (gpointer)queue);
  }

  /* now update the K-element queue */
  guint64 *ts = g_new(guint64, 1);
  *ts = LRU_K_params->ts;
  g_queue_push_head(queue, (gpointer)ts);
  if (queue->length > LRU_K_params->maxK)
    g_free(g_queue_pop_tail(queue));

  if (g_hash_table_contains(LRU_K_params->cache_hashtable,
                            (gpointer)(req->obj_id_ptr)))
    return TRUE;
  else
    return FALSE;
}

void _LRU_K_update(cache_t *cache, request_t *req) {
  /* needs to update pq */
  struct LRU_K_params *LRU_K_params =
      (struct LRU_K_params *)(cache->cache_params);

  GQueue *queue = g_hash_table_lookup(LRU_K_params->ghost_hashtable,
                                      (gconstpointer)(req->obj_id_ptr));

  pq_node_t *node = (pq_node_t *)g_hash_table_lookup(
      LRU_K_params->cache_hashtable, (gconstpointer)(req->obj_id_ptr));

  pqueue_pri_t pri;
  if (queue->length < LRU_K_params->K)
    pri.pri1 = INITIAL_TS;
  else
    pri.pri1 = *(guint64 *)g_queue_peek_nth(queue, LRU_K_params->K - 1);

  pqueue_change_priority(LRU_K_params->pq, pri, (void *)node);
  return;
}

void _LRU_K_evict(cache_t *LRU_K) {
  /** pop one node from pq, remove it from cache_hashtable
   **/

  struct LRU_K_params *LRU_K_params =
      (struct LRU_K_params *)(LRU_K->cache_params);

  pq_node_t *node = (pq_node_t *)pqueue_pop(LRU_K_params->pq);
  g_hash_table_remove(LRU_K_params->cache_hashtable,
                      (gconstpointer)(node->obj_id));
}

gboolean LRU_K_add(cache_t *cache, request_t *req) {
  struct LRU_K_params *LRU_K_params =
      (struct LRU_K_params *)(cache->cache_params);

  gboolean retval;
  LRU_K_params->ts++;
  if (LRU_K_check(cache, req)) {
    _LRU_K_update(cache, req);
    retval = TRUE;
  } else {
    _LRU_K_insert(cache, req);
    if ((long)g_hash_table_size(LRU_K_params->cache_hashtable) >
        cache->core.size)
      _LRU_K_evict(cache);
    retval = FALSE;
  }

  cache->core.req_cnt += 1;
  return retval;
}

void LRU_K_destroy(cache_t *cache) {
  struct LRU_K_params *LRU_K_params =
      (struct LRU_K_params *)(cache->cache_params);

  g_hash_table_destroy(LRU_K_params->cache_hashtable);
  g_hash_table_destroy(LRU_K_params->ghost_hashtable);
  pqueue_free(LRU_K_params->pq);

  cache_struct_free(cache);
}

//void LRU_K_destroy_unique(cache_t *cache) {
//  /* the difference between destroy_cloned_cache and destroy
//   is that the former one only free the resources that are
//   unique to the cacheAlgo, freeing these resources won't affect
//   other caches copied from original cacheAlgo
//   in Optimal, next_access should not be freed in destroy_cloned_cache,
//   because it is shared between different caches copied from the original one.
//   */
//  struct LRU_K_params *LRU_K_params =
//      (struct LRU_K_params *)(cache->cache_params);
//
//  g_hash_table_destroy(LRU_K_params->cache_hashtable);
//  g_hash_table_destroy(LRU_K_params->ghost_hashtable);
//  pqueue_free(LRU_K_params->pq);
//  cache_destroy_unique(cache);
//}

cache_t *LRU_K_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("LRUK", ccache_params);
  cache->cache_params = g_new0(struct LRU_K_params, 1);
  struct LRU_K_params *LRU_K_params =
      (struct LRU_K_params *)(cache->cache_params);

  cache->core.cache_specific_init_params = cache_specific_init_params;

  LRU_K_params->ts = 0;
  LRU_K_params->pq =
      pqueue_init((size_t)ccache_params.cache_size, cmp_pri, get_pri, set_pri, get_pos, set_pos);

  int K, maxK;
  K = ((struct LRU_K_init_params *)cache_specific_init_params)
          ->K; // because in gqueue, sequence begins with 0
  maxK = ((struct LRU_K_init_params *)cache_specific_init_params)->maxK;

  LRU_K_params->K = K;
  LRU_K_params->maxK = maxK;

  if (ccache_params.obj_id_type == OBJ_ID_NUM) {
    // don't use pqueue_node_destroyer here, because the obj_id inside node is
    // going to be freed by ghost_hashtable key
    LRU_K_params->cache_hashtable = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, g_free);
    LRU_K_params->ghost_hashtable =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, gqueue_destroyer);
  } else if (ccache_params.obj_id_type == OBJ_ID_STR) {
    // don't use pqueue_node_destroyer here, because the obj_id inside node is
    // going to be freed by ghost_hashtable key
    LRU_K_params->cache_hashtable = g_hash_table_new_full(
        g_str_hash, g_str_equal, NULL, g_free);
    LRU_K_params->ghost_hashtable =
        g_hash_table_new_full(g_str_hash, g_str_equal,
                              g_free, gqueue_destroyer);
  } else {
    ERROR("does not support given obj_id type: %c\n", ccache_params.obj_id_type);
  }

  return cache;
}

#ifdef __cplusplus
}
#endif
