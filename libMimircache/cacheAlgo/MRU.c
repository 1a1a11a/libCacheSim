//
//  MRU.c
//  libMimircache
//
//  MRU cacheAlgo replacement policy
//
//  Created by Juncheng on 8/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//


#ifdef _cplusplus
extern "C" {
#endif


#include "MRU.h"
#include "../../include/mimircache/cacheOp.h"


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
    if ((long) g_hash_table_size(MRU_params->hashtable) < cache->core.size)
      _MRU_insert(cache, req);
    retval = FALSE;
  }
  cache->core.req_cnt += 1;
  return retval;
}

void MRU_destroy(cache_t *cache) {
  struct MRU_params *MRU_params = (struct MRU_params *) (cache->cache_params);

  g_hash_table_destroy(MRU_params->hashtable);
  cache_struct_free(cache);
}

void MRU_destroy_unique(cache_t *cache) {
  /* the difference between destroy_cloned_cache and destroy
   is that the former one only free the resources that are
   unique to the cacheAlgo, freeing these resources won't affect
   other caches copied from original cacheAlgo
   in MRU, next_access should not be freed in destroy_cloned_cache,
   because it is shared between different caches copied from the original one.
   */
  MRU_destroy(cache);
}

cache_t *MRU_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("MRU", ccache_params);
  struct MRU_params *MRU_params = g_new0(struct MRU_params, 1);
  cache->cache_params = (void *) MRU_params;
  MRU_params->hashtable = create_hash_table_with_obj_id_type(ccache_params.obj_id_type, NULL, NULL, g_free, NULL);
  return cache;
}

#ifdef _cplusplus
}
#endif
