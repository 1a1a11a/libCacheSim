//
//  Random.c
//  libMimircache
//
//  Random cacheAlgo replacement policy
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//


#ifdef __cplusplus
extern "C" {
#endif

#include "Random.h"
#include "../../include/mimircache/cacheOp.h"


cache_t *Random_init(guint64 size, obj_id_type_t obj_id_type, void *params) {
  cache_t *cache = cache_struct_init("Random", size, obj_id_type);
  Random_params_t *Random_params = g_new0(Random_params_t, 1);
  cache->cache_params = (void *)Random_params;

  Random_params->hashtable = create_hash_table_with_obj_id_type(obj_id_type, NULL, cache_obj_destroyer, g_free, cache_obj_destroyer);

//  Random_params->array = g_array_sized_new(FALSE, FALSE, sizeof(gpointer), (guint)1000000);
  Random_params->array = g_ptr_array_new();

  time_t t;
  srand((unsigned)time(&t));
  return cache;
}


void Random_free(cache_t *cache) {
  Random_params_t *Random_params = (Random_params_t *)(cache->cache_params);

  g_hash_table_destroy(Random_params->hashtable);
  g_ptr_array_free(Random_params->array, TRUE);
  cache_struct_free(cache);
}


gboolean Random_get(cache_t *cache, request_t *req) {
  gboolean found_in_cache = Random_check(cache, req);
  if (found_in_cache) {
    _Random_update(cache, req);
  } else {
    _Random_insert(cache, req);
  }

  while (cache->core->used_size > cache->core->size)
    _Random_evict(cache, req);

  cache->core->req_cnt += 1;
  return found_in_cache;
}


void _Random_insert(cache_t *cache, request_t *req) {
  Random_params_t *Random_params = (Random_params_t *)(cache->cache_params);

  cache->core->used_size += req->obj_size;
  cache_obj_t *cache_obj = create_cache_obj_from_req(req);

  g_ptr_array_add(Random_params->array, cache_obj->obj_id_ptr);
  g_hash_table_insert(Random_params->hashtable, cache_obj->obj_id_ptr, cache_obj);

}

gboolean Random_check(cache_t *cache, request_t *req) {
  Random_params_t *Random_params = (Random_params_t *)(cache->cache_params);
  return g_hash_table_contains(Random_params->hashtable, (gconstpointer)(req->obj_id_ptr));
}

void _Random_update(cache_t *cache, request_t *req) {
  Random_params_t *Random_params = (Random_params_t *) (cache->cache_params);
  cache_obj_t *cache_obj = g_hash_table_lookup(Random_params->hashtable, req->obj_id_ptr);
  cache->core->used_size -= cache_obj->size;
  cache->core->used_size += req->obj_size;
  update_cache_obj(cache_obj, req);
}

void _Random_evict(cache_t *cache, request_t *req) {
  Random_params_t *Random_params = (Random_params_t *)(cache->cache_params);
  guint64 pos = rand();
  if (RAND_MAX < Random_params->array->len)
    pos = pos * rand();
  pos = pos % Random_params->array->len;

  gpointer obj_id_ptr_evict = g_ptr_array_index(Random_params->array, pos);

  cache_obj_t *cache_obj = g_hash_table_lookup(Random_params->hashtable, obj_id_ptr_evict);
  cache->core->used_size -= cache_obj->size;

  g_hash_table_remove(Random_params->hashtable, obj_id_ptr_evict);
  g_ptr_array_remove_index_fast(Random_params->array, pos);
}


gpointer _Random_evict_with_return(cache_t *cache, request_t *req) {
  /* the line below is to ensure that the randomized index can be large
 * enough, because RAND_MAX is only guaranteed to be 32767 */
  Random_params_t *Random_params = (Random_params_t *)(cache->cache_params);
  guint64 pos = (rand() * rand() * rand()) % (Random_params->array->len);

  gpointer obj_id_ptr_evict = g_ptr_array_index(Random_params->array, pos);
  if (req->obj_id_type == OBJ_ID_STR) {
    obj_id_ptr_evict = (gpointer) g_strdup((gchar *) obj_id_ptr_evict);
  }

  g_hash_table_remove(Random_params->hashtable, obj_id_ptr_evict);
  g_ptr_array_remove_index_fast(Random_params->array, pos);

  return obj_id_ptr_evict;
}


#ifdef __cplusplus
}
#endif
