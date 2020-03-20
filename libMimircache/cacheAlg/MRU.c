//
//  MRU.c
//  mimircache
//
//  MRU cacheAlg replacement policy
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "MRU.h"

#ifdef _cplusplus
extern "C" {
#endif

void _MRU_insert(cache_t *MRU, request_t *req) {
  struct MRU_params *MRU_params = (struct MRU_params *) (MRU->cache_params);

  gpointer key = req->obj_id_ptr;
  if (req->obj_id_type == OBJ_ID_STR) {
    key = (gpointer) g_strdup((gchar *) (req->obj_id_ptr));
  }
  g_hash_table_add(MRU_params->hashtable, (gpointer) key);
}

gboolean MRU_check(cache_t *cache, request_t *req) {
  return g_hash_table_contains(
    ((struct MRU_params *) (cache->cache_params))->hashtable,
    (gconstpointer) (req->obj_id_ptr));
}

void _MRU_update(cache_t *cache, request_t *req) { ; }

void _MRU_evict(cache_t *cache, request_t *req) { ; }

gboolean MRU_add(cache_t *cache, request_t *req) {
  struct MRU_params *MRU_params = (struct MRU_params *) (cache->cache_params);
  gboolean retval;

  if (MRU_check(cache, req)) {
    retval = TRUE;
  } else {
    if ((long) g_hash_table_size(MRU_params->hashtable) < cache->core->size)
      _MRU_insert(cache, req);
    retval = FALSE;
  }
  cache->core->ts += 1;
  return retval;
}

void MRU_destroy(cache_t *cache) {
  struct MRU_params *MRU_params = (struct MRU_params *) (cache->cache_params);

  g_hash_table_destroy(MRU_params->hashtable);
  cache_destroy(cache);
}

void MRU_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cacheAlg, freeing these resources won't affect
   other caches copied from original cacheAlg
   in MRU, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */
  MRU_destroy(cache);
}

cache_t *MRU_init(guint64 size, obj_id_t obj_id_type, void *params) {
  cache_t *cache = cache_init("MRU", size, obj_id_type);
  struct MRU_params *MRU_params = g_new0(struct MRU_params, 1);
  cache->cache_params = (void *) MRU_params;

  cache->core->cache_init = MRU_init;
  cache->core->destroy = MRU_destroy;
  cache->core->destroy_unique = MRU_destroy_unique;
  cache->core->add = MRU_add;
  cache->core->check = MRU_check;
//  cacheAlg->core->add_only = MRU_add;

  if (obj_id_type == OBJ_ID_NUM) {
    MRU_params->hashtable = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, NULL);
  } else if (obj_id_type == OBJ_ID_STR) {
    MRU_params->hashtable = g_hash_table_new_full(
      g_str_hash, g_str_equal, g_free, NULL);
  } else {
    ERROR("does not support given obj_id type: %c\n", obj_id_type);
  }

  return cache;
}

#ifdef _cplusplus
}
#endif
