//
//  Random.c
//  libMimircache
//
//  Random cacheAlg replacement policy
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "Random.h"

#ifdef __cplusplus
extern "C" {
#endif

void _Random_insert(cache_t *Random, request_t *req) {
  struct Random_params *Random_params = (struct Random_params *)(Random->cache_params);

  gpointer key = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    key = (gpointer)g_strdup((gchar *)(req->obj_id_ptr));
  }

  if ((long)g_hash_table_size(Random_params->hashtable) < Random->core->size) {
    // no eviction, just insert
    g_array_append_val(Random_params->array, key);
    g_hash_table_add(Random_params->hashtable, key);
  } else {
    // replace randomly chosen element
    guint64 pos = rand() % (Random->core->size);
    /* the line below is to ensure that the randomized index can be large
    *enough,
    **   because RAND_MAX is only guaranteed to be 32767
    */
    pos = (rand() % (Random->core->size) * pos) % (Random->core->size);
    gpointer evict_key = g_array_index(Random_params->array, gpointer, pos);
    g_hash_table_remove(Random_params->hashtable, evict_key);
    g_array_index(Random_params->array, gpointer, pos) = key;
    g_hash_table_add(Random_params->hashtable, (gpointer)key);
  }
}

gboolean Random_check(cache_t *cache, request_t *req) {
  return g_hash_table_contains(
      ((struct Random_params *)(cache->cache_params))->hashtable,
      (gconstpointer)(req->obj_id_ptr));
}

void _Random_update(cache_t *cache, request_t *req) { ; }

void _Random_evict(cache_t *cache, request_t *req) { ; }

gboolean Random_add(cache_t *cache, request_t *req) {
  gboolean retval;
  if (Random_check(cache, req)) {
    retval = TRUE;
  } else {
    _Random_insert(cache, req);
    retval = FALSE;
  }
  cache->core->req_cnt += 1;
  return retval;
}

void Random_destroy(cache_t *cache) {
  struct Random_params *Random_params =
      (struct Random_params *)(cache->cache_params);

  g_hash_table_destroy(Random_params->hashtable);
  g_array_free(Random_params->array, TRUE);
  cache_destroy(cache);
}

void Random_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cacheAlg, freeing these resources won't affect
   other caches copied from original cacheAlg
   in Random, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */
  Random_destroy(cache);
}

cache_t *Random_init(guint64 size, obj_id_t obj_id_type, void *params) {
  cache_t *cache = cache_init("Random", size, obj_id_type);
  struct Random_params *Random_params = g_new0(struct Random_params, 1);
  cache->cache_params = (void *)Random_params;

  cache->core->cache_init = Random_init;
  cache->core->destroy = Random_destroy;
  cache->core->destroy_unique = Random_destroy_unique;
  cache->core->add = Random_add;
  cache->core->check = Random_check;
//  cacheAlg->core->add_only = Random_add;

  if (obj_id_type == OBJ_ID_NUM) {
    Random_params->hashtable = g_hash_table_new_full(
        g_direct_hash, g_direct_equal, NULL, NULL);
  }

  else if (obj_id_type == OBJ_ID_STR) {
    Random_params->hashtable = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, NULL);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }

  Random_params->array =
      g_array_sized_new(FALSE, FALSE, sizeof(gpointer), (guint)size);

  time_t t;
  srand((unsigned)time(&t));
  return cache;
}

#ifdef __cplusplus
}
#endif
