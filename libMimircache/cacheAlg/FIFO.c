//
//  FIFO.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "FIFO.h"

#ifdef __cplusplus
extern "C" {
#endif

void _FIFO_insert(cache_t *FIFO, request_t *req) {
  struct FIFO_params *FIFO_params = (struct FIFO_params *)(FIFO->cache_params);

  gpointer key = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    key = (gpointer)g_strdup((gchar *)(req->obj_id_ptr));
  }
  g_hash_table_add(FIFO_params->hashtable, (gpointer)key);
  // store in a reversed order
  g_queue_push_tail(FIFO_params->list, (gpointer)key);
}

gboolean FIFO_check(cache_t *cache, request_t *req) {
  struct FIFO_params *FIFO_params = (struct FIFO_params *)(cache->cache_params);
  return g_hash_table_contains(FIFO_params->hashtable, req->obj_id_ptr);
}

void _FIFO_update(cache_t *FIFO, request_t *req) { return; }

void _FIFO_evict(cache_t *FIFO, request_t *req) {
  struct FIFO_params *FIFO_params = (struct FIFO_params *)(FIFO->cache_params);
  gpointer data = g_queue_pop_head(FIFO_params->list);
  g_hash_table_remove(FIFO_params->hashtable, (gconstpointer)data);
}

gpointer _FIFO_evict_with_return(cache_t *FIFO, request_t *req) {
  struct FIFO_params *FIFO_params = (struct FIFO_params *)(FIFO->cache_params);
  gpointer data = g_queue_pop_head(FIFO_params->list);
  gpointer gp = data;
  if (req->obj_id_type == OBJ_ID_STR) {
    gp = g_strdup((gchar *)data);
  }
  g_hash_table_remove(FIFO_params->hashtable, (gconstpointer)data);

  return gp;
}

gboolean FIFO_add(cache_t *cache, request_t *req) {
  struct FIFO_params *FIFO_params = (struct FIFO_params *)(cache->cache_params);
  gboolean retval;

  if (FIFO_check(cache, req)) {
    retval = TRUE;
  } else {
    _FIFO_insert(cache, req);
    if ((long)g_hash_table_size(FIFO_params->hashtable) > cache->core->size)
      _FIFO_evict(cache, req);
    retval = FALSE;
  }
  cache->core->req_cnt += 1;
  return retval;
}


void FIFO_destroy(cache_t *cache) {
  struct FIFO_params *FIFO_params = (struct FIFO_params *)(cache->cache_params);

  g_queue_free(FIFO_params->list);
  g_hash_table_destroy(FIFO_params->hashtable);
  cache_destroy(cache);
}

void FIFO_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cacheAlg, freeing these resources won't affect
   other caches copied from original cacheAlg
   in Optimal, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */

  FIFO_destroy(cache);
}

cache_t *FIFO_init(guint64 size, obj_id_t obj_id_type, void *params) {
  cache_t *cache = cache_init("FIFO", size, obj_id_type);
  cache->cache_params = g_new0(struct FIFO_params, 1);
  struct FIFO_params *FIFO_params = (struct FIFO_params *)(cache->cache_params);

  cache->core->cache_init = FIFO_init;
  cache->core->destroy = FIFO_destroy;
  cache->core->destroy_unique = FIFO_destroy_unique;
  cache->core->add = FIFO_add;
  cache->core->check = FIFO_check;
  cache->core->_evict = _FIFO_evict;
  cache->core->_insert = _FIFO_insert;
  cache->core->_update = _FIFO_update;
  cache->core->evict_with_return = _FIFO_evict_with_return;
  cache->core->get_used_size = FIFO_get_used_size;

  cache->core->cache_init_params = NULL;

  if (obj_id_type == OBJ_ID_NUM) {
    FIFO_params->hashtable = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (obj_id_type == OBJ_ID_STR) {
    FIFO_params->hashtable = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, NULL);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }
  FIFO_params->list = g_queue_new();

  return cache;
}

guint64 FIFO_get_used_size(cache_t *cache) {
  struct FIFO_params *FIFO_params = (struct FIFO_params *)(cache->cache_params);
  return (guint64)g_hash_table_size(FIFO_params->hashtable);
}

#ifdef __cplusplus
}
#endif
