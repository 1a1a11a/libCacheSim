//
//  this is the version using glib hashtable
//  compared to LRU, this uses more memory and is slower
//
//
//  LRUv0.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include <assert.h>

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/LRUv0.h"
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LRUv0_params {
  GHashTable *hashtable;
  GQueue *list;
} LRUv0_params_t;

cache_t *LRUv0_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LRUv0", ccache_params);
  cache->cache_init = LRUv0_init;
  cache->cache_free = LRUv0_free;
  cache->get = LRUv0_get;
  cache->check = LRUv0_check;
  cache->insert = LRUv0_insert;
  cache->evict = LRUv0_evict;
  cache->remove = LRUv0_remove;
  cache->to_evict = LRUv0_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->per_obj_metadata_size = 8 * 2;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  if (cache_specific_params != NULL) {
    printf("LRUv0 does not support any parameters, but got %s\n",
           cache_specific_params);
    abort();
  }

  cache->eviction_params = g_new0(LRUv0_params_t, 1);
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  LRUv0_params->hashtable =
      g_hash_table_new_full(g_int64_hash, g_direct_equal, NULL, NULL);
  LRUv0_params->list = g_queue_new();
  return cache;
}

void LRUv0_free(cache_t *cache) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  g_hash_table_destroy(LRUv0_params->hashtable);
  g_queue_free_full(LRUv0_params->list, (GDestroyNotify)free_cache_obj);
  cache_struct_free(cache);
}

cache_ck_res_e LRUv0_check(cache_t *cache, const request_t *req,
                           const bool update_cache) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  GList *node = (GList *)g_hash_table_lookup(LRUv0_params->hashtable,
                                             GSIZE_TO_POINTER(req->obj_id));
  if (node == NULL) return cache_ck_miss;

  if (!update_cache) return cache_ck_hit;

  cache_obj_t *cache_obj = node->data;
  if (likely(update_cache)) {
    if (unlikely(cache_obj->obj_size != req->obj_size)) {
      cache->occupied_size -= cache_obj->obj_size;
      cache->occupied_size += req->obj_size;
      cache_obj->obj_size = req->obj_size;
    }
  }
  g_queue_unlink(LRUv0_params->list, node);
  g_queue_push_tail_link(LRUv0_params->list, node);
  return cache_ck_hit;
}

cache_ck_res_e LRUv0_get(cache_t *cache, const request_t *req) {
  cache_ck_res_e cache_check = LRUv0_check(cache, req, true);
  if (req->obj_size <= cache->cache_size) {
    if (cache_check == cache_ck_miss) LRUv0_insert(cache, req);

    while (cache->occupied_size > cache->cache_size)
      LRUv0_evict(cache, req, NULL);
  } else {
    WARN("req %lld: obj size %ld larger than cache size %ld\n",
         (long long)req->obj_id, (long)req->obj_size, (long)cache->cache_size);
  }
  return cache_check;
}

cache_obj_t *LRUv0_insert(cache_t *cache, const request_t *req) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);

  cache->occupied_size += req->obj_size + cache->per_obj_metadata_size;
  cache_obj_t *cache_obj = create_cache_obj_from_request(req);
  // TODO: use SList should be more memory efficient than queue which uses
  // doubly-linklist under the hood
  GList *node = g_list_alloc();
  node->data = cache_obj;
  g_queue_push_tail_link(LRUv0_params->list, node);
  g_hash_table_insert(LRUv0_params->hashtable,
                      GSIZE_TO_POINTER(cache_obj->obj_id), (gpointer)node);

  return cache_obj;
}

cache_obj_t *LRUv0_get_cached_obj(cache_t *cache, const request_t *req) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  GList *node = (GList *)g_hash_table_lookup(LRUv0_params->hashtable,
                                             GSIZE_TO_POINTER(req->obj_id));
  cache_obj_t *cache_obj = node->data;
  return cache_obj;
}

cache_obj_t *LRUv0_to_evict(cache_t *cache) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  return (cache_obj_t *)g_queue_peek_head(LRUv0_params->list);
}

void LRUv0_evict(cache_t *cache, const request_t *req,
                 cache_obj_t *evicted_obj) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);
  cache_obj_t *cache_obj = LRUv0_to_evict(cache);
  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_metadata_size);
  g_hash_table_remove(LRUv0_params->hashtable,
                      (gconstpointer)cache_obj->obj_id);
  free_cache_obj(cache_obj);
}

void LRUv0_remove(cache_t *cache, const obj_id_t obj_id) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *)(cache->eviction_params);

  GList *node = (GList *)g_hash_table_lookup(LRUv0_params->hashtable,
                                             (gconstpointer)obj_id);
  if (!node) {
    PRINT_ONCE("obj to remove is not in the cache\n");
    return;
  }
  cache_obj_t *cache_obj = (cache_obj_t *)(node->data);
  assert(cache->occupied_size >=
         cache_obj->obj_size + cache->per_obj_metadata_size);
  cache->occupied_size -=
      (((cache_obj_t *)(node->data))->obj_size + cache->per_obj_metadata_size);
  cache->n_obj -= 1;

  g_queue_delete_link(LRUv0_params->list, (GList *)node);
  g_hash_table_remove(LRUv0_params->hashtable, (gconstpointer)obj_id);
  free_cache_obj(cache_obj);
}

#ifdef __cplusplus
}
#endif