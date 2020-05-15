//
//  Optimal.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//


#ifdef __cplusplus
extern "C" {
#endif

#include "Optimal.h"



/******************* priority queue structs and def **********************/

static int cmp_pri(pqueue_pri_t next, pqueue_pri_t curr) {
  return (next.pri1 < curr.pri1);
}

static pqueue_pri_t get_pri(void *a) { return ((pq_node_t *) a)->pri; }

static void set_pri(void *a, pqueue_pri_t pri) { ((pq_node_t *) a)->pri = pri; }

static size_t get_pos(void *a) { return ((pq_node_t *) a)->pos; }

static void set_pos(void *a, size_t pos) { ((pq_node_t *) a)->pos = pos; }

/*************************** OPT related ****************************/

void _Optimal_insert(cache_t *Optimal, request_t *req) {
  Optimal_params_t *Optimal_params =
    (Optimal_params_t *) (Optimal->cache_params);

  pq_node_t *node = g_new(pq_node_t, 1);
  gpointer key = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    key = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }

  gint64 cur_ts = Optimal_params->ts - 1;
  node->obj_id_type = req->obj_id_type;
  node->obj_id = (gpointer) key;
  if ((gint) g_array_index(Optimal_params->next_access, gint, cur_ts) == -1)
    node->pri.pri1 = G_MAXINT64;
  else
    node->pri.pri1 =
      cur_ts + (gint) g_array_index(Optimal_params->next_access, gint, cur_ts);
  pqueue_insert(Optimal_params->pq, (void *) node);
  g_hash_table_insert(Optimal_params->hashtable, (gpointer) key, (gpointer) node);
}

gboolean Optimal_check(cache_t *cache, request_t *req) {
  return g_hash_table_contains(
    ((Optimal_params_t *) (cache->cache_params))->hashtable,
    (gconstpointer) (req->obj_id_ptr));
}

void _Optimal_update(cache_t *Optimal, request_t *req) {
  Optimal_params_t *Optimal_params =
    (Optimal_params_t *) (Optimal->cache_params);
  void *node;
  node = (void *) g_hash_table_lookup(Optimal_params->hashtable,
                                      (gconstpointer) (req->obj_id_ptr));
  pqueue_pri_t pri;

  gint64 cur_ts = Optimal_params->ts - 1;
  if ((gint) g_array_index(Optimal_params->next_access, gint, cur_ts) == -1)
    pri.pri1 = G_MAXINT64;
  else
    pri.pri1 =
      cur_ts + (gint) g_array_index(Optimal_params->next_access, gint, cur_ts);

  pqueue_change_priority(Optimal_params->pq, pri, node);
}

void _Optimal_evict(cache_t *Optimal, request_t *req) {
  Optimal_params_t *Optimal_params =
    (Optimal_params_t *) (Optimal->cache_params);

  pq_node_t *node = (pq_node_t *) pqueue_pop(Optimal_params->pq);
  g_hash_table_remove(Optimal_params->hashtable, (gconstpointer) (node->obj_id));
}

void *_Optimal_evict_with_return(cache_t *Optimal, request_t *req) {
  Optimal_params_t *Optimal_params =
    (Optimal_params_t *) (Optimal->cache_params);

  pq_node_t *node = (pq_node_t *) pqueue_pop(Optimal_params->pq);
  void *evicted_key = node->obj_id;

  if (req->obj_id_type == OBJ_ID_STR) {
    evicted_key = (gpointer) g_strdup((gchar *) (node->obj_id));
  }

  g_hash_table_remove(Optimal_params->hashtable, (gconstpointer) (node->obj_id));
  return evicted_key;
}

guint64 Optimal_get_size(cache_t *cache) {
  Optimal_params_t *Optimal_params = (Optimal_params_t *) (cache->cache_params);
  return (guint64) g_hash_table_size(Optimal_params->hashtable);
}

gboolean Optimal_add(cache_t *cache, request_t *req) {
  Optimal_params_t *Optimal_params = (Optimal_params_t *) (cache->cache_params);
  gboolean retval;
  (Optimal_params->ts)++;

  if (Optimal_check(cache, req)) {
    _Optimal_update(cache, req);
    retval = TRUE;
  } else {
    _Optimal_insert(cache, req);
    retval = FALSE;
  }
  while ((long) g_hash_table_size(Optimal_params->hashtable) > cache->core.size)
    _Optimal_evict(cache, req);
  cache->core.req_cnt += 1;
  return retval;
}


gboolean Optimal_add_only(cache_t *cache, request_t *req) {
  Optimal_params_t *Optimal_params = (Optimal_params_t *) (cache->cache_params);
  (Optimal_params->ts)++; // do not move
  gboolean retval;

  if (Optimal_check(cache, req)) {
    _Optimal_update(cache, req);
    retval = TRUE;
  } else {
    _Optimal_insert(cache, req);
    if ((long) g_hash_table_size(Optimal_params->hashtable) > cache->core.size)
      _Optimal_evict(cache, req);
    retval = FALSE;
  }
  cache->core.req_cnt += 1;
  return retval;
}

void Optimal_destroy(cache_t *cache) {
  Optimal_params_t *Optimal_params = (Optimal_params_t *) (cache->cache_params);

  g_hash_table_destroy(Optimal_params->hashtable);
  pqueue_free(Optimal_params->pq);
  g_array_free(Optimal_params->next_access, TRUE);
  ((struct Optimal_init_params *) (cache->core.cache_specific_init_params))
    ->next_access = NULL;

  cache_struct_free(cache);
}

void Optimal_destroy_unique(cache_t *cache) {
  /* the difference between destroy_cloned_cache and destroy
   is that the former one only free the resources that are
   unique to the cacheAlgo, freeing these resources won't affect
   other caches copied from original cacheAlgo
   in Optimal, next_access should not be freed in destroy_cloned_cache,
   because it is shared between different caches copied from the original one.
   */

  Optimal_params_t *Optimal_params = (Optimal_params_t *) (cache->cache_params);
  g_hash_table_destroy(Optimal_params->hashtable);
  pqueue_free(Optimal_params->pq);
  g_free(cache->cache_params);
//  g_free(cache->core);
  g_free(cache);
}

cache_t *Optimal_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("Optimal", ccache_params);

  Optimal_params_t *Optimal_params = g_new0(Optimal_params_t, 1);
  cache->cache_params = (void *) Optimal_params;


  Optimal_params->ts = ((struct Optimal_init_params *) cache_specific_init_params)->ts;

  reader_t *reader = ((struct Optimal_init_params *) cache_specific_init_params)->reader;
  Optimal_params->reader = reader;

  Optimal_params->hashtable = create_hash_table_with_obj_id_type(ccache_params.obj_id_type, NULL, g_free, g_free, g_free);
  Optimal_params->pq = pqueue_init(ccache_params.cache_size, cmp_pri, get_pri, set_pri, get_pos, set_pos);

  if (((struct Optimal_init_params *) cache_specific_init_params)->next_access == NULL) {
    get_num_of_req(reader);
    Optimal_params->next_access = g_array_sized_new(
      FALSE, FALSE, sizeof(gint), (guint) reader->base->n_total_req);
    GArray *array = Optimal_params->next_access;
    gint64* dist = get_last_access_dist(reader);
    for (guint64 i=0; i<get_num_of_req(reader); i++){
          g_array_append_val(array, dist[i]);
    }

//    GSList *list = _get_last_access_dist(reader, read_one_req_above);
//    if (list == NULL) {
//      ERROR("error getting last access distance in Optimal_init\n");
//      exit(1);
//    }
//    GSList *list_move = list;

//    gint dist = (GPOINTER_TO_INT(list_move->data));
//    g_array_append_val(array, dist);
//    while ((list_move = g_slist_next(list_move)) != NULL) {
//      dist = (GPOINTER_TO_INT(list_move->data));
//      g_array_append_val(array, dist);
//    }
//    g_slist_free(list);

    ((struct Optimal_init_params *) cache_specific_init_params)->next_access =
      Optimal_params->next_access;
  } else
    Optimal_params->next_access =
      ((struct Optimal_init_params *) cache_specific_init_params)->next_access;

  cache->core.cache_specific_init_params = cache_specific_init_params;

  return cache;
}

#ifdef __cplusplus
}
#endif
