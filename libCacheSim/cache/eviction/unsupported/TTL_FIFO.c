//
//  a TTL_FIFO module that eviction objects according to TTL, if same TTL, evict using FIFO
//  it has the same behavior when TTL is constant
//
//
//  TTL_FIFO.c
//  libCacheSim
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//


#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include "TTL_FIFO.h"
#include "../utils/include/utilsInternal.h"


static inline int _get_idx(gint32 ts){
  return ts>MAX_TTL?MAX_TTL-1:ts;
}

cache_t *TTL_FIFO_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("TTL_FIFO", ccache_params);
  cache->cache_params = g_new0(TTL_FIFO_params_t, 1);
  TTL_FIFO_params_t *TTL_FIFO_params = (TTL_FIFO_params_t *) (cache->cache_params);
  TTL_FIFO_params->hashtable =
      g_hash_table_new_full(g_int64_hash, g_direct_equal, NULL, NULL);
  for (gint32 i=0; i<MAX_TTL; i++)
    TTL_FIFO_params->exp_time_array[i] = g_queue_new();
  TTL_FIFO_params->cur_ttl_evict_pos = 0;
  TTL_FIFO_params->start_ts = -1;
  return cache;
}

void TTL_FIFO_free(cache_t *cache) {
  TTL_FIFO_params_t *TTL_FIFO_params = (TTL_FIFO_params_t *) (cache->cache_params);
  g_hash_table_destroy(TTL_FIFO_params->hashtable);
  for (gint32 i=0; i<MAX_TTL; i++)
    g_queue_free_full(TTL_FIFO_params->exp_time_array[i], cache_obj_destroyer);
  cache_struct_free(cache);
}

void _TTL_FIFO_update(cache_t* cache, request_t *req){
  ERROR("TTL_FIFO needs default_ttl params\n");
  abort();
}

gboolean TTL_FIFO_get(cache_t* cache, request_t *req){
  ERROR("TTL_FIFO needs default_ttl params\n");
  abort();
}

gboolean TTL_FIFO_check(cache_t* cache, request_t *req){
  ERROR("TTL_FIFO needs default_ttl params\n");
  abort();
}

void _TTL_FIFO_insert(cache_t *cache, request_t *req) {
  TTL_FIFO_params_t *TTL_FIFO_params = (TTL_FIFO_params_t *) (cache->cache_params);
  if (TTL_FIFO_params->start_ts == -1)
    TTL_FIFO_params->start_ts = req->clock_time;

  cache->used_size += req->obj_size;
  cache_obj_t *cache_obj = create_cache_obj_from_request(req);
  gint32 idx = _get_idx(req->clock_time+req->ttl - TTL_FIFO_params->start_ts);
  if (idx < TTL_FIFO_params->cur_ttl_evict_pos)
    TTL_FIFO_params->cur_ttl_evict_pos = idx;

  GList *node = g_list_alloc();
  node->data = cache_obj;
  g_queue_push_tail_link(TTL_FIFO_params->exp_time_array[idx], node);
  g_hash_table_insert(TTL_FIFO_params->hashtable, cache_obj->obj_id_ptr, (gpointer) node);
}

void _TTL_FIFO_evict(cache_t *cache, request_t *req) {
  TTL_FIFO_params_t *TTL_FIFO_params = (TTL_FIFO_params_t *) (cache->cache_params);
  while (g_queue_get_length(TTL_FIFO_params->exp_time_array[TTL_FIFO_params->cur_ttl_evict_pos]) == 0){
    TTL_FIFO_params->cur_ttl_evict_pos += 1;
    if (TTL_FIFO_params->cur_ttl_evict_pos >= MAX_TTL){
      ERROR("cur_ttl_evict_pos is too large\n");
      abort();
    }
  }

  cache_obj_t *cache_obj = (cache_obj_t *) g_queue_pop_head(TTL_FIFO_params->exp_time_array[TTL_FIFO_params->cur_ttl_evict_pos]);
  assert (cache->used_size >= cache_obj->obj_size);
  cache->used_size -= cache_obj->obj_size;
  g_hash_table_remove(TTL_FIFO_params->hashtable, (gconstpointer) cache_obj->obj_id_ptr);
  destroy_cache_obj(cache_obj);
}

void TTL_FIFO_remove_obj(cache_t *cache, void *obj_id_ptr) {
  TTL_FIFO_params_t *TTL_FIFO_params = (TTL_FIFO_params_t *) (cache->cache_params);

  GList *node = (GList *) g_hash_table_lookup(TTL_FIFO_params->hashtable, obj_id_ptr);
  if (!node) {
    ERROR("obj to remove is not in the cache\n");
    abort();
  }
  cache_obj_t *cache_obj = (cache_obj_t *) (node->data);
  assert (cache->used_size >= cache_obj->obj_size);
  cache->used_size -= cache_obj->obj_size;
  g_queue_delete_link(TTL_FIFO_params->exp_time_array[cache_obj->exp_time-TTL_FIFO_params->start_ts], (GList *) node);
  g_hash_table_remove(TTL_FIFO_params->hashtable, obj_id_ptr);
  destroy_cache_obj(cache_obj);
}


/************************************* TTL ***************************************/
cache_check_result_t TTL_FIFO_check_and_update_with_ttl(cache_t *cache, request_t* req){
  TTL_FIFO_params_t *TTL_FIFO_params = (TTL_FIFO_params_t *) (cache->cache_params);
  cache_check_result_t result = cache_miss_e;
  GList *node = (GList *) g_hash_table_lookup(TTL_FIFO_params->hashtable, req->obj_id_ptr);
  if (node != NULL) {
    cache_obj_t *cache_obj = node->data;
    if (cache_obj->exp_time < req->clock_time) {
      /* obj is expired */
      result = expired_e;
      gint32 old_idx = _get_idx(cache_obj->exp_time-TTL_FIFO_params->start_ts);
      cache_obj->exp_time = req->clock_time + req->ttl;
      gint32 new_idx = _get_idx(cache_obj->exp_time-TTL_FIFO_params->start_ts);
      g_queue_unlink(TTL_FIFO_params->exp_time_array[old_idx], (GList *) node);
      g_queue_push_tail_link(TTL_FIFO_params->exp_time_array[new_idx], (GList *) node);
    } else {
      result = cache_hit_e;
    }
  }
  return result;
}

gboolean TTL_FIFO_get_with_ttl(cache_t* cache, request_t *req){
  req->ttl == 0 && (req->ttl = cache->default_ttl);
  cache_check_result_t cache_check = TTL_FIFO_check_and_update_with_ttl(cache, req);
  gboolean found_in_cache = cache_check == cache_hit_e;

  if (req->obj_size <= cache->size) {
    if (cache_check == cache_miss_e){
      _TTL_FIFO_insert(cache, req);
    }

    while (cache->used_size > cache->size)
      _TTL_FIFO_evict(cache, req);
  } else {
    WARN("req %lld: obj size %ld larger than cache size %ld\n", (long long) cache->req_cnt,
            (long) req->obj_size, (long) cache->size);
  }
  cache->req_cnt += 1;
  return found_in_cache;
}


#ifdef __cplusplus
extern "C" {
#endif
