//
//  myLRU.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "myLRU.h"

#ifdef __cplusplus
extern "C" {
#endif


cache_t *myLRU_init(guint64 size, obj_id_type_t obj_id_type, void *params) {
  cache_t *cache = cache_struct_init("myLRU", size, obj_id_type);
  cache->cache_params = g_new0(myLRU_params_t, 1);
  myLRU_params_t *myLRU_params = (myLRU_params_t *) (cache->cache_params);
  if (obj_id_type == OBJ_ID_NUM) {
    myLRU_params->hashtable = g_hash_table_new_full(
        g_int64_hash, g_int64_equal, g_free, NULL);
  } else if (obj_id_type == OBJ_ID_STR) {
    myLRU_params->hashtable = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, NULL);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }  myLRU_params->list = g_queue_new();
  return cache;
}

void myLRU_free(cache_t *cache) {
  myLRU_params_t *myLRU_params = (myLRU_params_t *) (cache->cache_params);
  g_hash_table_destroy(myLRU_params->hashtable);
  g_queue_free_full(myLRU_params->list, cache_obj_destroyer);
  cache_struct_free(cache);
}

gboolean myLRU_check(cache_t *cache, request_t *req) {
  myLRU_params_t *myLRU_params = (myLRU_params_t *) (cache->cache_params);
  return g_hash_table_contains(myLRU_params->hashtable, req->obj_id_ptr);
}

gboolean myLRU_get(cache_t *cache, request_t *req) {
  gboolean found_in_cache = myLRU_check(cache, req);
  if (req->obj_size <= cache->core->size) {
    if (found_in_cache)
      _myLRU_update(cache, req);
    else
      _myLRU_insert(cache, req);

    while (cache->core->used_size > cache->core->size)
      _myLRU_evict(cache, req);
  } else {
    WARNING("req %lld: obj size %ld larger than cache size %ld\n", (long long)cache->core->req_cnt,
            (long) req->obj_size, (long) cache->core->size);
  }
  cache->core->req_cnt += 1;
  return found_in_cache;
}

void _myLRU_insert(cache_t *cache, request_t *req) {
  myLRU_params_t *myLRU_params = (myLRU_params_t *) (cache->cache_params);
  cache->core->used_size += req->obj_size;
  cache_obj_t* cache_obj = create_cache_obj_from_req(req);

  GList *node = g_list_alloc();
  node->data = cache_obj;
  g_queue_push_tail_link(myLRU_params->list, node);
  g_hash_table_insert(myLRU_params->hashtable, cache_obj->obj_id_ptr, (gpointer) node);
}

void _myLRU_update(cache_t *cache, request_t *req) {
  myLRU_params_t *myLRU_params = (myLRU_params_t *) (cache->cache_params);
  GList *node = (GList *) g_hash_table_lookup(myLRU_params->hashtable, req->obj_id_ptr);

  cache_obj_t *cache_obj = node->data;
  cache->core->used_size -= cache_obj->size;
  cache->core->used_size += req->obj_size;
  update_cache_obj(cache_obj, req);
  g_queue_unlink(myLRU_params->list, node);
  g_queue_push_tail_link(myLRU_params->list, node);
}

cache_obj_t *myLRU_get_cached_obj(cache_t *cache, request_t *req) {
  myLRU_params_t *myLRU_params = (myLRU_params_t *) (cache->cache_params);
  GList *node = (GList *) g_hash_table_lookup(myLRU_params->hashtable, req->obj_id_ptr);
  cache_obj_t *cache_obj = node->data;
  return cache_obj;
}

void _myLRU_evict(cache_t *cache, request_t *req) {
  myLRU_params_t *myLRU_params = (myLRU_params_t *) (cache->cache_params);
  cache_obj_t *cache_obj = (cache_obj_t *) g_queue_pop_head(myLRU_params->list);

  cache->core->used_size -= cache_obj->size;
  g_hash_table_remove(myLRU_params->hashtable, (gconstpointer) cache_obj->obj_id_ptr);
  destroy_cache_obj(cache_obj);
}

gpointer _myLRU_evict_with_return(cache_t *cache, request_t *req) {
  myLRU_params_t *myLRU_params = (myLRU_params_t *) (cache->cache_params);
  cache_obj_t *cache_obj = g_queue_pop_head(myLRU_params->list);
  cache->core->used_size -= cache_obj->size;
  gpointer evicted_key = cache_obj->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    evicted_key = (gpointer) g_strdup((gchar *) (cache_obj->obj_id_ptr));
  }
  g_hash_table_remove(myLRU_params->hashtable, (gconstpointer) (cache_obj->obj_id_ptr));
  destroy_cache_obj(cache_obj);
  return evicted_key;
}


void myLRU_remove_obj(cache_t *cache, gpointer obj_id_ptr) {
  myLRU_params_t *myLRU_params = (myLRU_params_t *) (cache->cache_params);

  GList *node = (GList *) g_hash_table_lookup(myLRU_params->hashtable, obj_id_ptr);
  if (!node) {
    ERROR("obj to remove is not in the cache\n");
    abort();
  }
  cache_obj_t* cache_obj = (cache_obj_t *) (node->data);
  cache->core->used_size -= ((cache_obj_t *) (node->data))->size;
  g_queue_delete_link(myLRU_params->list, (GList *) node);
  g_hash_table_remove(myLRU_params->hashtable, obj_id_ptr);
  destroy_cache_obj(cache_obj);
}
#ifdef __cplusplus
}
#endif
