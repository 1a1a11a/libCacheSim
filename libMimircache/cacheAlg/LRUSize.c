//
//  a LRU module that supports different obj size
//
//
//  LRUSize.c
//  libMimircache
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//


#ifdef __cplusplus
extern "C" {
#endif


#include "LRUSize.h"
#include "../../include/mimircache/cacheOp.h"


void _LRUSize_insert(cache_t *cache, request_t *req) {
  LRUSize_params_t *LRUSize_params = (struct LRUSize_params *) (cache->cache_params);
  cache_obj_t *cache_obj = g_new(cache_obj_t, 1);
  cache_obj->size = req->size;
  cache_obj->extra_data = req->extra_data_ptr;
  cache_obj->obj_id = req->obj_id_ptr;
#ifdef TRACK_ACCESS_TIME
    cache_obj->access_time = LRUSize_params->logical_ts;
#endif
  cache->core->used_size += req->size;

  if (req->obj_id_type == OBJ_ID_STR) {
    cache_obj->obj_id = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }

  GList *node = g_list_alloc();
  node->data = cache_obj;
  g_queue_push_tail_link(LRUSize_params->list, node);
  g_hash_table_insert(LRUSize_params->hashtable, cache_obj->obj_id, (gpointer) node);
}

gboolean LRUSize_check(cache_t *cache, request_t *req) {
  LRUSize_params_t *LRUSize_params = (struct LRUSize_params *) (cache->cache_params);
  return g_hash_table_contains(LRUSize_params->hashtable, req->obj_id_ptr);
}

void _LRUSize_update(cache_t *cache, request_t *req) {
  LRUSize_params_t *LRUSize_params = (struct LRUSize_params *) (cache->cache_params);
  GList *node = (GList *) g_hash_table_lookup(LRUSize_params->hashtable, req->obj_id_ptr);

  cache_obj_t *cache_obj = node->data;
  if (cache->core->used_size < cache_obj->size) {
    ERROR("occupied cacheAlg size %llu smaller than object size %llu\n",
          (unsigned long long) cache->core->used_size,
          (unsigned long long) cache_obj->size);
    abort();
  }
  cache->core->used_size -= cache_obj->size;
  cache->core->used_size += req->size;
  cache_obj->size = req->size;
  // we shouldn't update extra_data_ptr here, otherwise, the old extra_data_ptr will be (memory) leaked
  // cache_obj->extra_data_ptr = req->extra_data_ptr;
#ifdef TRACK_ACCESS_TIME
  //  cache_obj->access_time = cacheAlg->core->req_cnt;
    cache_obj->access_time = LRUSize_params->logical_ts;
#endif
  g_queue_unlink(LRUSize_params->list, node);
  g_queue_push_tail_link(LRUSize_params->list, node);
}

void LRUSize_update_cached_data(cache_t *cache, request_t *req, void *extra_data) {
  LRUSize_params_t *LRUSize_params = (struct LRUSize_params *) (cache->cache_params);
  GList *node = (GList *) g_hash_table_lookup(LRUSize_params->hashtable, req->obj_id_ptr);
  cache_obj_t *cache_obj = node->data;
  if (cache_obj->extra_data != NULL)
    g_free(cache_obj->extra_data);
  cache_obj->extra_data = extra_data;
}

void *LRUSize_get_cached_data(cache_t *cache, request_t *req) {
  LRUSize_params_t *LRUSize_params = (struct LRUSize_params *) (cache->cache_params);
  GList *node = (GList *) g_hash_table_lookup(LRUSize_params->hashtable, req->obj_id_ptr);
  cache_obj_t *cache_obj = node->data;
  return cache_obj->extra_data;
}

void _LRUSize_evict(cache_t *cache, request_t *req) {
  LRUSize_params_t *LRUSize_params =
    (struct LRUSize_params *) (cache->cache_params);

  cache_obj_t *cache_obj =
    (cache_obj_t *) g_queue_pop_head(LRUSize_params->list);

  if (cache->core->used_size < cache_obj->size) {
    ERROR("occupied cacheAlg size %llu smaller than object size %llu\n",
          (unsigned long long) cache->core->used_size,
          (unsigned long long) cache_obj->size);
    abort();
  }

#ifdef TRACK_EVICTION_AGE
  gint64 last_real_ts = (gint64) GPOINTER_TO_SIZE(g_hash_table_lookup(
      LRUSize_params->last_access_rtime_map, cache_obj->key));
  gint64 last_logical_ts = (gint64) GPOINTER_TO_SIZE(g_hash_table_lookup(
      LRUSize_params->last_access_vtime_map, cache_obj->key));
  g_hash_table_remove(LRUSize_params->last_access_rtime_map, cache_obj->key);
  g_hash_table_remove(LRUSize_params->last_access_vtime_map, cache_obj->key);

  gint64 eviction_age_realtime = req->real_time - last_real_ts;
  gint64 eviction_age_logical_time = LRUSize_params->logical_ts - last_logical_ts;
  LRUSize_params->logical_eviction_age_sum += eviction_age_logical_time;
  LRUSize_params->real_eviction_age_sum += eviction_age_realtime;
  LRUSize_params->eviction_cnt ++;

  if (req->real_time % 100 == 0 && req->real_time != LRUSize_params->last_output_ts){
    fprintf(LRUSize_params->eviction_age_ofile, "%lld-%lld: %ld-%ld\n",
            (long long) req->real_time,
            (long long) LRUSize_params->logical_ts,
            (long)LRUSize_params->logical_eviction_age_sum/LRUSize_params->eviction_cnt,
            (long)LRUSize_params->real_eviction_age_sum/LRUSize_params->eviction_cnt);
    LRUSize_params->eviction_cnt = 0;
    LRUSize_params->logical_eviction_age_sum = 0;
    LRUSize_params->real_eviction_age_sum = 0;
    LRUSize_params->last_output_ts = req->real_time;
  }
#endif

  cache->core->used_size -= cache_obj->size;
  g_hash_table_remove(LRUSize_params->hashtable, (gconstpointer) cache_obj->obj_id);
  cacheobj_destroyer(cache_obj);
  if (cache_obj->extra_data)
    g_free(cache_obj->extra_data);
  g_free(cache_obj);
}

gpointer _LRUSize_evict_with_return(cache_t *cache, request_t *req) {
  /** evict one element and return the evicted element,
   * needs to free the memory of returned data
   */

  LRUSize_params_t *LRUSize_params =
    (struct LRUSize_params *) (cache->cache_params);

  cache_obj_t *cache_obj = g_queue_pop_head(LRUSize_params->list);
  if (cache->core->used_size < cache_obj->size) {
    ERROR("occupied cacheAlg size %llu smaller than object size %llu\n",
          (unsigned long long) cache->core->used_size,
          (unsigned long long) cache_obj->size);
    abort();
  }

  cache->core->used_size -= cache_obj->size;

  gpointer evicted_key = cache_obj->obj_id;
  if (req->obj_id_type == OBJ_ID_STR) {
    evicted_key = (gpointer) g_strdup((gchar *) (cache_obj->obj_id));
  }

  g_hash_table_remove(LRUSize_params->hashtable,
                      (gconstpointer) (cache_obj->obj_id));
  if (cache_obj->extra_data)
    g_free(cache_obj->extra_data);
  g_free(cache_obj);
  return evicted_key;
}

gboolean LRUSize_add(cache_t *cache, request_t *req) {
  LRUSize_params_t *LRUSize_params = (struct LRUSize_params *) (cache->cache_params);
  if (req->size == 0) {
    ERROR("LRUSize get size zero for request\n");
    abort();
  }

#ifdef TRACK_EVICTION_AGE
  g_hash_table_insert(LRUSize_params->last_access_rtime_map, req->obj_id_ptr,
                      GSIZE_TO_POINTER(req->real_time)+1);
//  printf("real time %lld\n", req->real_time);
  g_hash_table_insert(LRUSize_params->last_access_vtime_map, req->obj_id_ptr,
                      GSIZE_TO_POINTER(LRUSize_params->logical_ts));
#endif


  gboolean exist = LRUSize_check(cache, req);
  if (req->size <= cache->core->size) {
    if (exist)
      _LRUSize_update(cache, req);
    else
      _LRUSize_insert(cache, req);

    while (cache->core->used_size > cache->core->size)
      _LRUSize_evict(cache, req);
  } else {
    WARNING("obj size %ld larger than cacheAlg size %ld\n", (long) (req->size), cache->core->size);
  }


//  LRUSize_params->logical_ts++;
  cache->core->req_cnt += 1;

  return exist;
}

void LRUSize_destroy(cache_t *cache) {
  LRUSize_params_t *LRUSize_params =
    (struct LRUSize_params *) (cache->cache_params);

  g_hash_table_destroy(LRUSize_params->hashtable);
//  g_queue_free_full(LRUSize_params->list, g_free);
  g_queue_free_full(LRUSize_params->list, cacheobj_destroyer);
  //    cache_destroy(cacheAlg);
}

void LRUSize_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cacheAlg, freeing these resources won't affect
   other caches copied from original cacheAlg
   in Optimal, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */

  LRUSize_destroy(cache);
}

cache_t *LRUSize_init(guint64 size, obj_id_t obj_id_type, void *params) {
  cache_t *cache = cache_init("LRUSize", size, obj_id_type);
  cache->cache_params = g_new0(struct LRUSize_params, 1);
  LRUSize_params_t *LRUSize_params =
    (struct LRUSize_params *) (cache->cache_params);

  cache->core->cache_init = LRUSize_init;
  cache->core->destroy = LRUSize_destroy;
  cache->core->destroy_unique = LRUSize_destroy_unique;
  cache->core->add = LRUSize_add;
  cache->core->check = LRUSize_check;
  cache->core->get_cached_data = LRUSize_get_cached_data;
  cache->core->update_cached_data = LRUSize_update_cached_data;

  cache->core->_insert = _LRUSize_insert;
  cache->core->_update = _LRUSize_update;
  cache->core->_evict = _LRUSize_evict;
  cache->core->evict_with_return = _LRUSize_evict_with_return;
  cache->core->get_used_size = LRUSize_get_used_size;
  cache->core->get_objmap = LRUSize_get_objmap;
  cache->core->remove_obj = LRUSize_remove_obj;
  cache->core->cache_init_params = NULL;

  if (obj_id_type == OBJ_ID_NUM) {
    LRUSize_params->hashtable = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (obj_id_type == OBJ_ID_STR) {
    LRUSize_params->hashtable = g_hash_table_new_full(
      g_str_hash, g_str_equal, g_free, NULL);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }
  LRUSize_params->list = g_queue_new();

#ifdef TRACK_EVICTION_AGE
  LRUSize_params->eviction_cnt = 0;
  LRUSize_params->last_output_ts = 0;
  LRUSize_params->logical_eviction_age_sum = 0;
  LRUSize_params->real_eviction_age_sum = 0;
  LRUSize_params->eviction_age_ofile = fopen((char *)params, "w");
  LRUSize_params->last_access_rtime_map = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
  LRUSize_params->last_access_vtime_map = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
#endif

  return cache;
}

void LRUSize_remove_obj(cache_t *cache, void *data_to_remove) {
  LRUSize_params_t *LRUSize_params =
    (struct LRUSize_params *) (cache->cache_params);

  GList *node =
    (GList *) g_hash_table_lookup(LRUSize_params->hashtable, data_to_remove);
  if (!node) {
    fprintf(stderr,
            "LRUSize_remove_obj: data to remove is not in the cacheAlg\n");
    abort();
  }
  if (cache->core->used_size < ((cache_obj_t *) (node->data))->size) {
    ERROR("occupied cacheAlg size %llu smaller than object size %llu\n",
          (unsigned long long) cache->core->used_size,
          (unsigned long long) ((cache_obj_t *) (node->data))->size);
    abort();
  }

  cache->core->used_size -= ((cache_obj_t *) (node->data))->size;
  g_queue_delete_link(LRUSize_params->list, (GList *) node);
  g_hash_table_remove(LRUSize_params->hashtable, data_to_remove);
}

guint64 LRUSize_get_used_size(cache_t *cache) {
  return (guint64) cache->core->used_size;
}

GHashTable *LRUSize_get_objmap(cache_t *cache) {
  LRUSize_params_t *LRUSize_params =
    (struct LRUSize_params *) (cache->cache_params);
  return LRUSize_params->hashtable;
}

#ifdef __cplusplus
extern "C" {
#endif
