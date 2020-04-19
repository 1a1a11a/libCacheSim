//
// Created by Juncheng Yang on 3/20/20.
//

#ifdef __cplusplus
extern "C" {
#endif


#include "include/LRUComplex.h"
#include "../../include/mimircache/cacheOp.h"


void _LRUComplex_insert(cache_t *cache, request_t *req) {
  LRUComplex_params_t *LRUComplex_params = (struct LRUComplex_params *) (cache->cache_params);
  cache_obj_t *cache_obj = g_new(cache_obj_t, 1);
  cache_obj->size = req->size;
  cache_obj->extra_data_ptr = req->extra_data_ptr;
  cache_obj->obj_id_ptr = req->obj_id_ptr;
#ifdef TRACK_ACCESS_TIME
  cache_obj->access_time = LRUComplex_params->logical_ts;
#endif
  cache->core->used_size += req->size;

  if (req->obj_id_type == OBJ_ID_STR) {
    cache_obj->obj_id_ptr = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }

  GList *node = g_list_alloc();
  node->data = cache_obj;
  g_queue_push_tail_link(LRUComplex_params->list, node);
  g_hash_table_insert(LRUComplex_params->hashtable, cache_obj->obj_id_ptr, (gpointer) node);
}

gboolean LRUComplex_check(cache_t *cache, request_t *req) {
  LRUComplex_params_t *LRUComplex_params = (struct LRUComplex_params *) (cache->cache_params);
  return g_hash_table_contains(LRUComplex_params->hashtable, req->obj_id_ptr);
}

void _LRUComplex_update(cache_t *cache, request_t *req) {
  LRUComplex_params_t *LRUComplex_params = (struct LRUComplex_params *) (cache->cache_params);
  GList *node = (GList *) g_hash_table_lookup(LRUComplex_params->hashtable, req->obj_id_ptr);

  cache_obj_t *cache_obj = node->data;
  if (cache->core->used_size < cache_obj->size) {
    ERROR("occupied cacheAlgo size %llu smaller than object size %llu\n",
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
  //  cache_obj->access_time = cacheAlgo->core->req_cnt;
    cache_obj->access_time = LRUComplex_params->logical_ts;
#endif
  g_queue_unlink(LRUComplex_params->list, node);
  g_queue_push_tail_link(LRUComplex_params->list, node);
}


void _LRUComplex_evict(cache_t *cache, request_t *req) {
  LRUComplex_params_t *LRUComplex_params =
      (struct LRUComplex_params *) (cache->cache_params);

  cache_obj_t *cache_obj =
      (cache_obj_t *) g_queue_pop_head(LRUComplex_params->list);

  if (cache->core->used_size < cache_obj->size) {
    ERROR("occupied cacheAlgo size %llu smaller than object size %llu\n",
          (unsigned long long) cache->core->used_size,
          (unsigned long long) cache_obj->size);
    abort();
  }

#ifdef TRACK_EVICTION_AGE
  gint64 last_real_ts = (gint64) GPOINTER_TO_SIZE(g_hash_table_lookup(
      LRUComplex_params->last_access_rtime_map, cache_obj->key));
  gint64 last_logical_ts = (gint64) GPOINTER_TO_SIZE(g_hash_table_lookup(
      LRUComplex_params->last_access_vtime_map, cache_obj->key));
  g_hash_table_remove(LRUComplex_params->last_access_rtime_map, cache_obj->key);
  g_hash_table_remove(LRUComplex_params->last_access_vtime_map, cache_obj->key);

  gint64 eviction_age_realtime = req->real_time - last_real_ts;
  gint64 eviction_age_logical_time = LRUComplex_params->logical_ts - last_logical_ts;
  LRUComplex_params->logical_eviction_age_sum += eviction_age_logical_time;
  LRUComplex_params->real_eviction_age_sum += eviction_age_realtime;
  LRUComplex_params->eviction_cnt ++;

  if (req->real_time % 100 == 0 && req->real_time != LRUComplex_params->last_output_ts){
    fprintf(LRUComplex_params->eviction_age_ofile, "%lld-%lld: %ld-%ld\n",
            (long long) req->real_time,
            (long long) LRUComplex_params->logical_ts,
            (long)LRUComplex_params->logical_eviction_age_sum/LRUComplex_params->eviction_cnt,
            (long)LRUComplex_params->real_eviction_age_sum/LRUComplex_params->eviction_cnt);
    LRUComplex_params->eviction_cnt = 0;
    LRUComplex_params->logical_eviction_age_sum = 0;
    LRUComplex_params->real_eviction_age_sum = 0;
    LRUComplex_params->last_output_ts = req->real_time;
  }
#endif

  cache->core->used_size -= cache_obj->size;
  g_hash_table_remove(LRUComplex_params->hashtable, (gconstpointer) cache_obj->obj_id_ptr);
  destroy_cache_obj(cache_obj);
  if (cache_obj->extra_data)
    g_free(cache_obj->extra_data_ptr);
  g_free(cache_obj);
}

gpointer _LRUComplex_evict_with_return(cache_t *cache, request_t *req) {
  /** evict one element and return the evicted element,
   * needs to free the memory of returned data
   */

  LRUComplex_params_t *LRUComplex_params =
      (struct LRUComplex_params *) (cache->cache_params);

  cache_obj_t *cache_obj = g_queue_pop_head(LRUComplex_params->list);
  if (cache->core->used_size < cache_obj->size) {
    ERROR("occupied cacheAlgo size %llu smaller than object size %llu\n",
          (unsigned long long) cache->core->used_size,
          (unsigned long long) cache_obj->size);
    abort();
  }

  cache->core->used_size -= cache_obj->size;

  gpointer evicted_key = cache_obj->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    evicted_key = (gpointer) g_strdup((gchar *) (cache_obj->obj_id_ptr));
  }

  g_hash_table_remove(LRUComplex_params->hashtable,
                      (gconstpointer) (cache_obj->obj_id_ptr));
  if (cache_obj->extra_data)
    g_free(cache_obj->extra_data_ptr);
  g_free(cache_obj);
  return evicted_key;
}

gboolean LRUComplex_add(cache_t *cache, request_t *req) {
  LRUComplex_params_t *LRUComplex_params = (struct LRUComplex_params *) (cache->cache_params);
  if (req->size == 0) {
    ERROR("LRUComplex get size zero for request\n");
    abort();
  }

#ifdef TRACK_EVICTION_AGE
  g_hash_table_insert(LRUComplex_params->last_access_rtime_map, req->obj_id_ptr,
                      GSIZE_TO_POINTER(req->real_time)+1);
//  printf("real time %lld\n", req->real_time);
  g_hash_table_insert(LRUComplex_params->last_access_vtime_map, req->obj_id_ptr,
                      GSIZE_TO_POINTER(LRUComplex_params->logical_ts));
#endif


  gboolean exist = LRUComplex_check(cache, req);
  if (req->size <= cache->core->size) {
    if (exist)
      _LRUComplex_update(cache, req);
    else
      _LRUComplex_insert(cache, req);

    while (cache->core->used_size > cache->core->size)
      _LRUComplex_evict(cache, req);
  } else {
    WARNING("obj size %ld larger than cacheAlgo size %ld\n", (long) (req->size), cache->core->size);
  }


//  LRUComplex_params->logical_ts++;
  cache->core->req_cnt += 1;

  return exist;
}

void LRUComplex_destroy(cache_t *cache) {
  LRUComplex_params_t *LRUComplex_params =
      (struct LRUComplex_params *) (cache->cache_params);

  g_hash_table_destroy(LRUComplex_params->hashtable);
//  g_queue_free_full(LRUComplex_params->list, g_free);
  g_queue_free_full(LRUComplex_params->list, cache_obj_destroyer);
  //    cache_struct_free(cacheAlgo);
}

void LRUComplex_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cacheAlgo, freeing these resources won't affect
   other caches copied from original cacheAlgo
   in Optimal, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */

  LRUComplex_destroy(cache);
}

cache_t *LRUComplex_init(guint64 size, obj_id_type_t obj_id_type, void *params) {
  cache_t *cache = cache_struct_init("LRUComplex", size, obj_id_type);
  cache->cache_params = g_new0(struct LRUComplex_params, 1);
  LRUComplex_params_t *LRUComplex_params =
      (struct LRUComplex_params *) (cache->cache_params);

  if (obj_id_type == OBJ_ID_NUM) {
    LRUComplex_params->hashtable = g_hash_table_new_full(
        g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (obj_id_type == OBJ_ID_STR) {
    LRUComplex_params->hashtable = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, NULL);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }
  LRUComplex_params->list = g_queue_new();

#ifdef TRACK_EVICTION_AGE
  LRUComplex_params->eviction_cnt = 0;
  LRUComplex_params->last_output_ts = 0;
  LRUComplex_params->logical_eviction_age_sum = 0;
  LRUComplex_params->real_eviction_age_sum = 0;
  LRUComplex_params->eviction_age_ofile = fopen((char *)params, "w");
  LRUComplex_params->last_access_rtime_map = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
  LRUComplex_params->last_access_vtime_map = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
#endif

  return cache;
}

void LRUComplex_remove_obj(cache_t *cache, void *data_to_remove) {
  LRUComplex_params_t *LRUComplex_params =
      (struct LRUComplex_params *) (cache->cache_params);

  GList *node =
      (GList *) g_hash_table_lookup(LRUComplex_params->hashtable, data_to_remove);
  if (!node) {
    fprintf(stderr,
            "LRUComplex_remove_obj: data to remove is not in the cacheAlgo\n");
    abort();
  }
  if (cache->core->used_size < ((cache_obj_t *) (node->data))->size) {
    ERROR("occupied cacheAlgo size %llu smaller than object size %llu\n",
          (unsigned long long) cache->core->used_size,
          (unsigned long long) ((cache_obj_t *) (node->data))->size);
    abort();
  }

  cache->core->used_size -= ((cache_obj_t *) (node->data))->size;
  g_queue_delete_link(LRUComplex_params->list, (GList *) node);
  g_hash_table_remove(LRUComplex_params->hashtable, data_to_remove);
}

guint64 LRUComplex_get_used_size(cache_t *cache) {
  return (guint64) cache->core->used_size;
}

GHashTable *LRUComplex_get_objmap(cache_t *cache) {
  LRUComplex_params_t *LRUComplex_params =
      (struct LRUComplex_params *) (cache->cache_params);
  return LRUComplex_params->hashtable;
}

#ifdef __cplusplus
extern "C" {
#endif
