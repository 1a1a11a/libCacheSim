//
// Created by Juncheng Yang on 4/23/20.
//

#ifndef libCacheSim_CACHEUTILS_H
#define libCacheSim_CACHEUTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/cacheObj.h"
#include <glib.h>

/****************** find obj expiration related info ******************/
static inline void _get_cache_state_ht_iter(cache_obj_t *cache_obj,
                                            gpointer user_data) {
  cache_state_t *cache_state = user_data;
  cache_state->stored_obj_cnt += 1;
  cache_state->used_bytes += cache_obj->obj_size;
#ifdef SUPPORT_TTL
  if (cache_obj->exp_time < cache_state->cur_time) {
    cache_state->expired_obj_cnt += 1;
    cache_state->expired_bytes += cache_obj->obj_size;
  }
#endif
}

static inline void get_cache_state(cache_t *cache, cache_state_t *cache_state) {
  hashtable_foreach(cache->hashtable, _get_cache_state_ht_iter,
                    cache_state);
}

#ifdef __cplusplus
}
#endif

#endif // libCacheSim_CACHEUTILS_H
