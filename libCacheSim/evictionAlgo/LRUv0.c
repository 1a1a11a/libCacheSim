//
//  this is the version using glib hashtable
//
//
//  LRUv0.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifdef __cplusplus
extern "C" {
#endif

#include "include/cacheAlg.h"
#include "../include/libCacheSim/evictionAlgo/LRUv0.h"
#include "../utils/include/utilsInternal.h"
#include <assert.h>

cache_t *LRUv0_init(common_cache_params_t ccache_params,
                    void *cache_specific_params) {
  cache_t *cache = cache_struct_init("LRUv0", ccache_params);
  cache->cache_params = g_new0(LRUv0_params_t, 1);
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *) (cache->cache_params);
  LRUv0_params->hashtable =
      create_hash_table_with_obj_id_type(OBJ_ID_NUM, NULL, NULL, g_free, NULL);
  LRUv0_params->list = g_queue_new();
  return cache;
}

void LRUv0_free(cache_t *cache) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *) (cache->cache_params);
  g_hash_table_destroy(LRUv0_params->hashtable);
  g_queue_free_full(LRUv0_params->list, (GDestroyNotify) free_cache_obj);
  cache_struct_free(cache);
}

cache_ck_res_e LRUv0_check(cache_t *cache, request_t *req, bool update_cache) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *) (cache->cache_params);
  GList *node = (GList *) g_hash_table_lookup(LRUv0_params->hashtable,
                                              GSIZE_TO_POINTER(req->obj_id_int));
  if (node == NULL)
    return cache_ck_miss;

  if (!update_cache)
    return cache_ck_hit;

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

cache_ck_res_e LRUv0_get(cache_t *cache, request_t *req) {
  cache_ck_res_e cache_check = LRUv0_check(cache, req, true);
  if (req->obj_size <= cache->cache_size) {
    if (cache_check == cache_ck_miss)
      LRUv0_insert(cache, req);

    while (cache->occupied_size > cache->cache_size)
      LRUv0_evict(cache, req, NULL);
  } else {
    WARNING("req %lld: obj size %ld larger than cache size %ld\n",
            (long long) cache->req_cnt, (long) req->obj_size,
            (long) cache->cache_size);
  }
  cache->req_cnt += 1;
  return cache_check;
}

void LRUv0_insert(cache_t *LRUv0, request_t *req) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *) (LRUv0->cache_params);
  LRUv0->occupied_size += req->obj_size;
  cache_obj_t *cache_obj = create_cache_obj_from_request(req);
  // TODO: use SList should be more memory efficient than queue which uses
  // doubly-linklist under the hood
  GList *node = g_list_alloc();
  node->data = cache_obj;
  g_queue_push_tail_link(LRUv0_params->list, node);
  g_hash_table_insert(LRUv0_params->hashtable,
                      GSIZE_TO_POINTER(cache_obj->obj_id_int), (gpointer) node);
}

cache_obj_t *LRUv0_get_cached_obj(cache_t *cache, request_t *req) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *) (cache->cache_params);
  GList *node = (GList *) g_hash_table_lookup(LRUv0_params->hashtable,
                                              GSIZE_TO_POINTER(req->obj_id_int));
  cache_obj_t *cache_obj = node->data;
  return cache_obj;
}

void LRUv0_evict(cache_t *LRUv0, request_t *req, cache_obj_t *evicted_obj) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *) (LRUv0->cache_params);
  cache_obj_t *cache_obj = (cache_obj_t *) g_queue_pop_head(LRUv0_params->list);
  assert(LRUv0->occupied_size >= cache_obj->obj_size);
  LRUv0->occupied_size -= cache_obj->obj_size;
  g_hash_table_remove(LRUv0_params->hashtable,
                      (gconstpointer) cache_obj->obj_id_int);
  free_cache_obj(cache_obj);
}

void LRUv0_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  LRUv0_params_t *LRUv0_params = (LRUv0_params_t *) (cache->cache_params);

  GList *node = (GList *) g_hash_table_lookup(LRUv0_params->hashtable,
                                              (gconstpointer) obj_to_remove->obj_id_int);
  if (!node) {
    ERROR("obj to remove is not in the cache\n");
    abort();
  }
  cache_obj_t *cache_obj = (cache_obj_t *) (node->data);
  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= ((cache_obj_t *) (node->data))->obj_size;
  g_queue_delete_link(LRUv0_params->list, (GList *) node);
  g_hash_table_remove(LRUv0_params->hashtable, (gconstpointer) obj_to_remove->obj_id_int);
  free_cache_obj(cache_obj);
}

#ifdef __cplusplus
extern "C" {
#endif