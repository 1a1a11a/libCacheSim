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

cache_t *FIFO_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("FIFO", ccache_params);
  cache->cache_params = g_new0(FIFO_params_t, 1);
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  FIFO_params->hashtable = create_hash_table_with_obj_id_type(ccache_params.obj_id_type, NULL, NULL, g_free, NULL);
  FIFO_params->list = g_queue_new();
  return cache;
}

void FIFO_free(cache_t *cache) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  g_hash_table_destroy(FIFO_params->hashtable);
  g_queue_free_full(FIFO_params->list, cache_obj_destroyer);
  cache_struct_free(cache);
}

gboolean FIFO_check(cache_t *cache, request_t *req) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  if (g_hash_table_contains(FIFO_params->hashtable, req->obj_id_ptr))
    return cache_hit_e;
  else
    return cache_miss_e;
}


gboolean FIFO_get(cache_t *cache, request_t *req) {
  cache_check_result_t cache_check = FIFO_check(cache, req);
  gboolean found_in_cache = cache_check == cache_hit_e;

  if (req->obj_size <= cache->core.size) {
    if (!found_in_cache)
      _FIFO_insert(cache, req);

    while (cache->core.used_size > cache->core.size)
      _FIFO_evict(cache, req);
  } else {
    WARNING("req %lld: obj size %ld larger than cache size %ld\n", (long long) cache->core.req_cnt,
            (long) req->obj_size, (long) cache->core.size);
  }
  cache->core.req_cnt += 1;
  return found_in_cache;
}

void _FIFO_insert(cache_t *cache, request_t *req) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  cache->core.used_size += req->obj_size;
  cache_obj_t *cache_obj = create_cache_obj_from_req(req);

  GList *node = g_list_alloc();
  node->data = cache_obj;
  g_queue_push_tail_link(FIFO_params->list, node);
  g_hash_table_insert(FIFO_params->hashtable, cache_obj->obj_id_ptr, (gpointer) node);
}

void _FIFO_update(cache_t *cache, request_t *req) {
  static guint64 n_warning = 0;
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  GList *node = (GList *) g_hash_table_lookup(FIFO_params->hashtable, req->obj_id_ptr);
  cache_obj_t *cache_obj = node->data;
  if (cache_obj->obj_size != req->obj_size){
    if (n_warning % 20000 == 0) {
      WARNING("detecting obj size change cache_obj size %u - req size %u (warning %llu)\n", cache_obj->obj_size, req->obj_size,
              (long long unsigned) n_warning);
      n_warning += 1;
    }
    cache->core.used_size -= cache_obj->obj_size;
    cache->core.used_size += req->obj_size;
    cache_obj->obj_size = req->obj_size;
  }
}

void _FIFO_evict(cache_t *cache, request_t *req) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  cache_obj_t *cache_obj = (cache_obj_t *) g_queue_pop_head(FIFO_params->list);
  assert (cache->core.used_size >= cache_obj->obj_size);
  cache->core.used_size -= cache_obj->obj_size;
  g_hash_table_remove(FIFO_params->hashtable, (gconstpointer) cache_obj->obj_id_ptr);
  destroy_cache_obj(cache_obj);
}

gpointer _FIFO_evict_with_return(cache_t *cache, request_t *req) {
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  cache_obj_t *cache_obj = g_queue_pop_head(FIFO_params->list);
  assert (cache->core.used_size >= cache_obj->obj_size);
  cache->core.used_size -= cache_obj->obj_size;
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
  assert (cache->core.used_size >= cache_obj->obj_size);
  cache->core.used_size -= cache_obj->obj_size;
  g_queue_delete_link(FIFO_params->list, (GList *) node);
  g_hash_table_remove(FIFO_params->hashtable, obj_id_ptr);
  destroy_cache_obj(cache_obj);
}


/************************************* TTL ***************************************/
cache_check_result_t FIFO_check_with_ttl(cache_t *cache, request_t* req){
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  GList *node = (GList *) g_hash_table_lookup(FIFO_params->hashtable, req->obj_id_ptr);
  if (node == NULL){
    return cache_miss_e;
  }
  cache_obj_t *cache_obj = node->data;
  if (cache_obj->exp_time < req->real_time){
    /* obj is expired */
    cache->stat.hit_expired_cnt += 1;
    cache->stat.hit_expired_byte += cache_obj->obj_size;
    assert (cache->core.used_size >= cache_obj->obj_size);
    cache->core.used_size -= cache_obj->obj_size;
    g_queue_delete_link(FIFO_params->list, (GList *) node);
    g_hash_table_remove(FIFO_params->hashtable, cache_obj->obj_id_ptr);
    destroy_cache_obj(cache_obj);
    return cache_miss_e;
  }
  return cache_hit_e;
}


cache_check_result_t FIFO_check_and_update_with_ttl(cache_t *cache, request_t* req){
  FIFO_params_t *FIFO_params = (FIFO_params_t *) (cache->cache_params);
  cache_check_result_t result = cache_miss_e;
  GList *node = (GList *) g_hash_table_lookup(FIFO_params->hashtable, req->obj_id_ptr);
  if (node != NULL) {
    cache_obj_t *cache_obj = node->data;
    if (cache_obj->exp_time < req->real_time) {
      /* obj is expired */
      result = expired_e;
      cache->stat.hit_expired_cnt += 1;
      cache->stat.hit_expired_byte += cache_obj->obj_size;
      cache_obj->exp_time = req->real_time + req->ttl;
    } else {
      result = cache_hit_e;
    }
  }
  return result;
}

gboolean FIFO_get_with_ttl(cache_t* cache, request_t *req){
  req->ttl == 0 && (req->ttl = cache->core.default_ttl);
  cache_check_result_t cache_check = FIFO_check_and_update_with_ttl(cache, req);
  gboolean found_in_cache = cache_check == cache_hit_e;

  if (req->obj_size <= cache->core.size) {
    if (cache_check == cache_miss_e){
      _FIFO_insert(cache, req);
    }

    while (cache->core.used_size > cache->core.size)
      _FIFO_evict(cache, req);
  } else {
    WARNING("req %lld: obj size %ld larger than cache size %ld\n", (long long) cache->core.req_cnt,
            (long) req->obj_size, (long) cache->core.size);
  }
  cache->core.req_cnt += 1;
  return found_in_cache;
}


#ifdef __cplusplus
extern "C" {
#endif
