//
//  a FIFO module that supports different obj size
//
//
//  FIFO.c
//  libMimircache
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//


#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include "FIFO.h"
#include "../utils/include/utilsInternal.h"

cache_t *FIFO_init(guint64 size, obj_id_type_t obj_id_type, void *params) {
  cache_t *cache = cache_struct_init("FIFO", size, obj_id_type);
  cache->cache_params = g_new0(FIFO_params_t, 1);
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);

  FIFO_params->hashtable = create_hash_table_with_obj_id_type(obj_id_type, NULL, NULL, g_free, NULL);
  FIFO_params->list = g_queue_new();

  return cache;
}


void FIFO_free(cache_t *cache) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  g_hash_table_destroy(FIFO_params->hashtable);
  g_queue_free_full(FIFO_params->list, cache_obj_destroyer);
  cache_struct_free(cache);
}


gboolean FIFO_get(cache_t *cache, request_t *req) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  gboolean found_in_cache = FIFO_check(cache, req);

  if (found_in_cache)
    _FIFO_update(cache, req);
  else
    _FIFO_insert(cache, req);

  while (cache->core->used_size > cache->core->size)
    _FIFO_evict(cache, req);

  cache->core->req_cnt += 1;
  return found_in_cache;
}


gboolean FIFO_check(cache_t *cache, request_t *req) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  return g_hash_table_contains(FIFO_params->hashtable, req->obj_id_ptr);
}



void _FIFO_insert(cache_t *cache, request_t *req) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  cache->core->used_size += req->obj_size;
  cache_obj_t *cache_obj = create_cache_obj_from_req(req);

  GList *node = g_list_alloc();
  node->data = cache_obj;
  g_queue_push_tail_link(FIFO_params->list, node);
  g_hash_table_insert(FIFO_params->hashtable, cache_obj->obj_id_ptr,
                      (gpointer) node);
}

void _FIFO_update(cache_t *cache, request_t *req) {
  static guint64 n_warning = 0;
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  GList *node = (GList *) g_hash_table_lookup(FIFO_params->hashtable, req->obj_id_ptr);
  cache_obj_t *cache_obj = node->data;
  if (cache_obj->size != (guint32) req->obj_size && n_warning % 20000 == 0) {
    WARNING("detecting obj size change cache_obj size %u - req size %u (warning %llu)\n", cache_obj->size, req->obj_size,
        (long long unsigned) n_warning);
    n_warning += 1;
  }
  cache->core->used_size -= cache_obj->size;
  cache->core->used_size += req->obj_size;
  update_cache_obj(cache_obj, req);
}

void _FIFO_evict(cache_t *cache, request_t *req) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  cache_obj_t *cache_obj = (cache_obj_t *) g_queue_pop_head(FIFO_params->list);
  assert (cache->core->used_size >= cache_obj->size);
  cache->core->used_size -= cache_obj->size;
  g_hash_table_remove(FIFO_params->hashtable, (gconstpointer) cache_obj->obj_id_ptr);
  destroy_cache_obj(cache_obj);
}

gpointer _FIFO_evict_with_return(cache_t *cache, request_t *req) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  cache_obj_t *cache_obj = g_queue_pop_head(FIFO_params->list);
  assert (cache->core->used_size >= cache_obj->size);
  cache->core->used_size -= cache_obj->size;
  gpointer evicted_key = cache_obj->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    evicted_key = (gpointer) g_strdup((gchar *) (cache_obj->obj_id_ptr));
  }

  g_hash_table_remove(FIFO_params->hashtable, (gconstpointer) (cache_obj->obj_id_ptr));
  destroy_cache_obj(cache_obj);
  return evicted_key;
}

void FIFO_remove_obj(cache_t *cache, void *obj_id_ptr) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);

  GList *node = (GList *) g_hash_table_lookup(FIFO_params->hashtable, obj_id_ptr);
  if (!node) {
    ERROR("obj to remove is not in the cacheAlgo\n");
    abort();
  }
  cache_obj_t *cache_obj = (cache_obj_t *) (node->data);
  assert (cache->core->used_size >= cache_obj->size);
  cache->core->used_size -= cache_obj->size;
  g_queue_delete_link(FIFO_params->list, (GList *) node);
  g_hash_table_remove(FIFO_params->hashtable, obj_id_ptr);
  destroy_cache_obj(cache_obj);
}


#ifdef __cplusplus
extern "C" {
#endif
