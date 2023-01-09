//
// Created by Juncheng Yang on 4/23/20.
//

#ifndef libCacheSim_CACHEUTILS_H
#define libCacheSim_CACHEUTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>

#include "../dataStructure/hashtable/hashtable.h"
#include "../include/libCacheSim/cache.h"
#include "../include/libCacheSim/cacheObj.h"

/****************** find obj expiration related info ******************/
static inline void _get_cache_state_ht_iter(cache_obj_t *cache_obj,
                                            gpointer user_data) {
  cache_stat_t *cache_state = user_data;
  cache_state->n_obj += 1;
  cache_state->occupied_byte += cache_obj->obj_size;
#ifdef SUPPORT_TTL
  if (cache_obj->exp_time < cache_state->curr_rtime) {
    cache_state->expired_obj_cnt += 1;
    cache_state->expired_bytes += cache_obj->obj_size;
  }
#endif
}

static inline void get_cache_state(cache_t *cache, cache_stat_t *cache_state) {
  if (cache->hashtable->n_obj == 0) return;

  hashtable_foreach(cache->hashtable, _get_cache_state_ht_iter, cache_state);
}

#ifdef __cplusplus
}
#endif

#endif  // libCacheSim_CACHEUTILS_H
