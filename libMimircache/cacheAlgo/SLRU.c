//
//  SLRU.h
//  libMimircache
//
//  Created by Juncheng on 2/12/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//

#include "SLRU.h"



#ifdef __cplusplus
extern "C" {
#endif

void _SLRU_insert(cache_t *SLRU, request_t *cp) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *) (SLRU->cache_params);
  _LRU_insert(SLRU_params->LRUs[0], cp);
  SLRU_params->current_sizes[0]++;
}

gboolean SLRU_check(cache_t *cache, request_t *cp) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *) (cache->cache_params);
  gboolean retVal = FALSE;
  int i;
  for (i = 0; i < SLRU_params->n_seg; i++)
    retVal = retVal || LRU_check(SLRU_params->LRUs[i], cp);
  return retVal;
}

void _SLRU_update(cache_t *cache, request_t *cp) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *) (cache->cache_params);
  int i;
  for (i = 0; i < SLRU_params->n_seg; i++) {
    if (LRU_check(SLRU_params->LRUs[i], cp)) {
      /* move to upper LRU
       * first remove from current LRU
       * then add to upper LRU,
       * if upper LRU is full, evict one, insert into current LRU
       */
      if (i != SLRU_params->n_seg - 1) {
        LRU_remove_obj(SLRU_params->LRUs[i], cp->obj_id_ptr);
        SLRU_params->current_sizes[i]--;
        _LRU_insert(SLRU_params->LRUs[i + 1], cp);
        SLRU_params->current_sizes[i + 1]++;
        if ((long) SLRU_params->LRUs[i + 1]->core.used_size  >
            SLRU_params->LRUs[i + 1]->core.size) {
          gpointer old_itemp = cp->obj_id_ptr;
          gpointer evicted =
            _LRU_evict_with_return(SLRU_params->LRUs[i + 1], cp);
          SLRU_params->current_sizes[i + 1]--;
          cp->obj_id_ptr = evicted;
          _LRU_insert(SLRU_params->LRUs[i], cp);
          SLRU_params->current_sizes[i]++;
          cp->obj_id_ptr = old_itemp;
          g_free(evicted);
        }
      } else {
        // this is the last segment, just update
        _LRU_update(SLRU_params->LRUs[i], cp);
      }
      return;
    }
  }
}

void _SLRU_evict(cache_t *SLRU, request_t *cp) {
  /* because insert only happens at LRU0,
   * then eviction also can only happens at LRU0
   */
  SLRU_params_t *SLRU_params = (SLRU_params_t *) (SLRU->cache_params);

  _LRU_evict(SLRU_params->LRUs[0], cp);
#ifdef SANITY_CHECK
  if ((long)LRU_get_size(SLRU_params->LRUs[0]) != SLRU_params->LRUs[0]->core.size) {
    fprintf(stderr,
            "ERROR: SLRU_evict, after eviction, LRU0 size %lu, "
            "full size %ld\n",
            (unsigned long)LRU_get_size(SLRU_params->LRUs[0]),
            SLRU_params->LRUs[0]->core.size);
    exit(1);
  }
#endif
}

gpointer _SLRU_evict_with_return(cache_t *SLRU, request_t *cp) {
  /** evict one element and return the evicted element,
   user needs to free the memory of returned data **/

  SLRU_params_t *SLRU_params = (SLRU_params_t *) (SLRU->cache_params);
  return _LRU_evict_with_return(SLRU_params->LRUs[0], cp);
}

gboolean SLRU_add(cache_t *cache, request_t *cp) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *) (cache->cache_params);
  gboolean retval;
  if (SLRU_check(cache, cp)) {
    _SLRU_update(cache, cp);
    retval = TRUE;
  } else {
    _SLRU_insert(cache, cp);
    if ((long) SLRU_params->LRUs[0]->core.used_size > SLRU_params->LRUs[0]->core.size)
      _SLRU_evict(cache, cp);
    retval = FALSE;
  }
  cache->core.req_cnt += 1;
  return retval;
}

void SLRU_destroy(cache_t *cache) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *) (cache->cache_params);
  int i;
  for (i = 0; i < SLRU_params->n_seg; i++)
    LRU_free(SLRU_params->LRUs[i]);
  g_free(SLRU_params->LRUs);
  g_free(SLRU_params->current_sizes);
  cache_struct_free(cache);
}

void SLRU_destroy_unique(cache_t *cache) {
  /* the difference between destroy_cloned_cache and destroy
   is that the former one only free the resources that are
   unique to the cacheAlgo, freeing these resources won't affect
   other caches copied from original cacheAlgo
   in Optimal, next_access should not be freed in destroy_cloned_cache,
   because it is shared between different caches copied from the original one.
   */

  SLRU_params_t *SLRU_params = (SLRU_params_t *) (cache->cache_params);
  int i;
  for (i = 0; i < SLRU_params->n_seg; i++)
    LRU_free(SLRU_params->LRUs[i]);
  g_free(SLRU_params->LRUs);
  g_free(SLRU_params->current_sizes);
  cache_struct_free(cache);
}

cache_t *SLRU_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("SLRU", ccache_params);
  cache->cache_params = g_new0(struct SLRU_params, 1);
  SLRU_params_t *SLRU_params = (SLRU_params_t *) (cache->cache_params);
  SLRU_init_params_t *init_params = (SLRU_init_params_t *) cache_specific_init_params;

  cache->core.cache_specific_init_params = cache_specific_init_params;

  SLRU_params->n_seg = init_params->n_seg;
  SLRU_params->current_sizes = g_new0(uint64_t, SLRU_params->n_seg);
  SLRU_params->LRUs = g_new(cache_t *, SLRU_params->n_seg);
  int i;
  ccache_params.cache_size /= SLRU_params->n_seg;
  for (i = 0; i < SLRU_params->n_seg; i++) {
    SLRU_params->LRUs[i] =
      LRU_init(ccache_params, NULL);
  }

  return cache;
}

guint64 SLRU_get_size(cache_t *cache) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *) (cache->cache_params);
  int i;
  guint64 size = 0;
  for (i = 0; i < SLRU_params->n_seg; i++)
    size += SLRU_params->current_sizes[i];
  return size;
}

#ifdef __cplusplus
}
#endif
