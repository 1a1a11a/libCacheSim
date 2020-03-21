//
//  LRU.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  major-rewrite Mar 2020
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifdef __cplusplus
extern "C" {
#endif


#include "LRU.h"
#include "../../include/mimircache/cacheOp.h"
#include "../../utils/include/utilsInternal.h"


void _LRU_insert(cache_t *LRU, request_t *req) {
  LRU_params_t *LRU_params = (LRU_params_t *) (LRU->cache_params);

  gpointer key = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    key = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }

  GList *node = g_list_alloc();
  node->data = key;

  g_queue_push_tail_link(LRU_params->list, node);
  g_hash_table_insert(LRU_params->hashtable, (gpointer) key, (gpointer) node);
}

gboolean LRU_check(cache_t *cache, request_t *req) {
  LRU_params_t *LRU_params = (LRU_params_t *) (cache->cache_params);
  return g_hash_table_contains(LRU_params->hashtable, req->obj_id_ptr);
}

void _LRU_update(cache_t *cache, request_t *req) {
  LRU_params_t *LRU_params = (LRU_params_t *) (cache->cache_params);
  GList *node =
    (GList *) g_hash_table_lookup(LRU_params->hashtable, req->obj_id_ptr);
  g_queue_unlink(LRU_params->list, node);
  g_queue_push_tail_link(LRU_params->list, node);
}

void _LRU_evict(cache_t *LRU, request_t *req) {
  LRU_params_t *LRU_params = (LRU_params_t *) (LRU->cache_params);
  gpointer data = g_queue_pop_head(LRU_params->list);
  g_hash_table_remove(LRU_params->hashtable, (gconstpointer) data);

}

gpointer _LRU_evict_with_return(cache_t *LRU, request_t *req) {
  /** evict one element and return the evicted element,
   * needs to free the memory of returned data
   */

  LRU_params_t *LRU_params = (LRU_params_t *) (LRU->cache_params);

  gpointer data = g_queue_pop_head(LRU_params->list);

  gpointer evicted_key = data;
  if (req->obj_id_type == OBJ_ID_STR) {
    evicted_key = (gpointer) g_strdup((gchar *) data);
  }

  g_hash_table_remove(LRU_params->hashtable, (gconstpointer) data);
  return evicted_key;
}

gboolean LRU_add(cache_t *cache, request_t *req) {
  LRU_params_t *LRU_params = (LRU_params_t *) (cache->cache_params);
  gboolean retval;
  if (LRU_check(cache, req)) {
    _LRU_update(cache, req);
    retval = TRUE;
  } else {
    _LRU_insert(cache, req);
    if ((long) g_hash_table_size(LRU_params->hashtable) > cache->core->size)
      _LRU_evict(cache, req);
    retval = FALSE;
  }
  LRU_params->ts++;
  cache->core->req_cnt += 1;
  return retval;
}


void LRU_destroy(cache_t *cache) {
  LRU_params_t *LRU_params = (LRU_params_t *) (cache->cache_params);

  //    g_queue_free(LRU_params->list);                 // Jason: should call
  //    g_queue_free_full to free the memory of node content
  // 0921
  g_queue_free(LRU_params->list);
  g_hash_table_destroy(LRU_params->hashtable);
  cache_destroy(cache);
}

void LRU_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cacheAlg, freeing these resources won't affect
   other caches copied from original cacheAlg
   in Optimal, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */

  LRU_destroy(cache);
}


cache_t *LRU_init(guint64 size, obj_id_t obj_id_type, void *params) {
  cache_t *cache = cache_init("LRU", size, obj_id_type);
  cache->cache_params = g_new0(LRU_params_t, 1);
  LRU_params_t *LRU_params = (LRU_params_t *) (cache->cache_params);

  LRU_params->hashtable = create_hash_table_with_obj_id_type(obj_id_type, NULL,
      NULL, g_free, NULL);
  LRU_params->list = g_queue_new();

  return cache;
}

void LRU_remove_obj(cache_t *cache, void *data_to_remove) {
  LRU_params_t *LRU_params = (LRU_params_t *) (cache->cache_params);

  gpointer data = g_hash_table_lookup(LRU_params->hashtable, data_to_remove);
  if (!data) {
    fprintf(stderr, "LRU_remove_obj: data to remove is not in the cacheAlg\n");
    exit(1);
  }
  g_queue_delete_link(LRU_params->list, (GList *) data);
  g_hash_table_remove(LRU_params->hashtable, data_to_remove);
}

guint64 LRU_get_used_size(cache_t *cache) {
  LRU_params_t *LRU_params = (LRU_params_t *) (cache->cache_params);
  return (guint64) g_hash_table_size(LRU_params->hashtable);
}

#ifdef __cplusplus
}
#endif
