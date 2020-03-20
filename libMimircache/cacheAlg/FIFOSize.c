//
//  a FIFO module that supports different obj size
//
//
//  FIFOSize.c
//  libMimircache
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "FIFOSize.h"

#ifdef __cplusplus
extern "C" {
#endif

void _FIFOSize_insert(cache_t *cache, request_t *req) {
  struct FIFOSize_params *FIFOSize_params =
      (struct FIFOSize_params *)(cache->cache_params);
  cache_obj_t *cache_obj = g_new(cache_obj_t, 1);
  cache_obj->size = req->size;
  cache->core->used_size += req->size;
  cache_obj->obj_id = req->obj_id_ptr; 
  
  if (req->obj_id_type == OBJ_ID_STR) {
    cache_obj->obj_id = (gpointer)g_strdup((gchar *)(req->obj_id_ptr));
  }

  GList *node = g_list_alloc();
  node->data = cache_obj;

  g_queue_push_tail_link(FIFOSize_params->list, node);
  g_hash_table_insert(FIFOSize_params->hashtable, cache_obj->obj_id,
                      (gpointer)node);
}

gboolean FIFOSize_check(cache_t *cache, request_t *req) {
  struct FIFOSize_params *FIFOSize_params =
      (struct FIFOSize_params *)(cache->cache_params);
  return g_hash_table_contains(FIFOSize_params->hashtable, req->obj_id_ptr);
}

void _FIFOSize_update(cache_t *cache, request_t *req) {
  struct FIFOSize_params *FIFOSize_params =
      (struct FIFOSize_params *)(cache->cache_params);
  GList *node =
      (GList *)g_hash_table_lookup(FIFOSize_params->hashtable, req->obj_id_ptr);

  cache_obj_t *cache_obj = node->data;
  if (cache_obj->size != req->size) {
    ERROR("updating in FIFO is impossible, it will only result in a new entry");
  }
}

void _FIFOSize_evict(cache_t *cache, request_t *req) {
  struct FIFOSize_params *FIFOSize_params =
      (struct FIFOSize_params *)(cache->cache_params);

  cache_obj_t *cache_obj =
      (cache_obj_t *)g_queue_pop_head(FIFOSize_params->list);

  if (cache->core->used_size < cache_obj->size) {
    ERROR("occupied cacheAlg size %llu smaller than object size %llu\n",
          (unsigned long long)cache->core->used_size,
          (unsigned long long)cache_obj->size);
    abort();
  }
  cache->core->used_size -= cache_obj->size;
  g_hash_table_remove(FIFOSize_params->hashtable,
                      (gconstpointer)cache_obj->obj_id);
  g_free(cache_obj);
}

gpointer _FIFOSize_evict_with_return(cache_t *cache, request_t *req) {
  /** evict one element and return the evicted element,
   * needs to free the memory of returned data
   */

  struct FIFOSize_params *FIFOSize_params =
      (struct FIFOSize_params *)(cache->cache_params);

  cache_obj_t *cache_obj = g_queue_pop_head(FIFOSize_params->list);
  if (cache->core->used_size < cache_obj->size) {
    ERROR("occupied cacheAlg size %llu smaller than object size %llu\n",
          (unsigned long long)cache->core->used_size,
          (unsigned long long)cache_obj->size);
    abort();
  }

  cache->core->used_size -= cache_obj->size;

  gpointer evicted_key = cache_obj->obj_id;
  if (req->obj_id_type == OBJ_ID_STR) {
    evicted_key = (gpointer)g_strdup((gchar *)(cache_obj->obj_id));
  }

  g_hash_table_remove(FIFOSize_params->hashtable,
                      (gconstpointer)(cache_obj->obj_id));
  g_free(cache_obj);
  return evicted_key;
}

gboolean FIFOSize_add(cache_t *cache, request_t *req) {
  struct FIFOSize_params *FIFOSize_params =
      (struct FIFOSize_params *)(cache->cache_params);
  if (req->size == 0) {
    ERROR("FIFOSize get size zero for request\n");
    abort();
  }

  gboolean exist = FIFOSize_check(cache, req);

  if (FIFOSize_check(cache, req))
    _FIFOSize_update(cache, req);
  else
    _FIFOSize_insert(cache, req);

  while (cache->core->used_size > cache->core->size)
    _FIFOSize_evict(cache, req);

  FIFOSize_params->ts++;
  cache->core->ts += 1;
  return exist;
}

void FIFOSize_destroy(cache_t *cache) {
  struct FIFOSize_params *FIFOSize_params =
      (struct FIFOSize_params *)(cache->cache_params);

  g_hash_table_destroy(FIFOSize_params->hashtable);
  //    g_queue_free(FIFOSize_params->list);
  g_queue_free_full(FIFOSize_params->list, g_free);
  cache_destroy(cache);
}

void FIFOSize_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cacheAlg, freeing these resources won't affect
   other caches copied from original cacheAlg
   in Optimal, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */

  FIFOSize_destroy(cache);
}

cache_t *FIFOSize_init(guint64 size, obj_id_t obj_id_type, void *params) {
  cache_t *cache = cache_init("FIFOSize", size, obj_id_type);
  cache->cache_params = g_new0(struct FIFOSize_params, 1);
  struct FIFOSize_params *FIFOSize_params =
      (struct FIFOSize_params *)(cache->cache_params);

  cache->core->cache_init = FIFOSize_init;
  cache->core->destroy = FIFOSize_destroy;
  cache->core->destroy_unique = FIFOSize_destroy_unique;
  cache->core->add = FIFOSize_add;
  cache->core->check = FIFOSize_check;
  cache->core->_insert = _FIFOSize_insert;
  cache->core->_update = _FIFOSize_update;
  cache->core->_evict = _FIFOSize_evict;
  cache->core->evict_with_return = _FIFOSize_evict_with_return;
  cache->core->get_current_size = FIFOSize_get_size;
  cache->core->get_objmap = FIFOSize_get_objmap;
//  cacheAlg->core->remove_obj = FIFOSize_remove_obj;
  cache->core->cache_init_params = NULL;

  if (obj_id_type == OBJ_ID_NUM) {
    FIFOSize_params->hashtable = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (obj_id_type == OBJ_ID_STR) {
    FIFOSize_params->hashtable = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, NULL);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }
  FIFOSize_params->list = g_queue_new();

  return cache;
}

void FIFOSize_remove(cache_t *cache, void *data_to_remove) {
  struct FIFOSize_params *FIFOSize_params =
      (struct FIFOSize_params *)(cache->cache_params);

  GList *node =
      (GList *)g_hash_table_lookup(FIFOSize_params->hashtable, data_to_remove);
  if (!node) {
    fprintf(stderr,
            "FIFOSize_remove: data to remove is not in the cacheAlg\n");
    abort();
  }
  if (cache->core->used_size < ((cache_obj_t *)(node->data))->size) {
    ERROR("occupied cacheAlg size %llu smaller than object size %llu\n",
          (unsigned long long)cache->core->used_size,
          (unsigned long long)((cache_obj_t *)(node->data))->size);
    abort();
  }

  cache->core->used_size -= ((cache_obj_t *)(node->data))->size;
  g_queue_delete_link(FIFOSize_params->list, (GList *)node);
  g_hash_table_remove(FIFOSize_params->hashtable, data_to_remove);
}

guint64 FIFOSize_get_size(cache_t *cache) {
  return (guint64)cache->core->used_size;
}

GHashTable *FIFOSize_get_objmap(cache_t *cache) {
  struct FIFOSize_params *FIFOSize_params =
      (struct FIFOSize_params *)(cache->cache_params);
  return FIFOSize_params->hashtable;
}

#ifdef __cplusplus
extern "C" {
#endif
