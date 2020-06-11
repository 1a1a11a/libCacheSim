//
// Created by Juncheng Yang on 4/23/20.
//

#ifndef LIBMIMIRCACHE_CACHEUTILS_H
#define LIBMIMIRCACHE_CACHEUTILS_H


#ifdef __cplusplus
extern "C"
{
#endif

#include <glib.h>
#include "../../include/mimircache/cacheObj.h"


/****************** find obj expiration related info ******************/
static inline void get_cache_state_ht_iter(cache_obj_t *cache_obj, gpointer user_data){
  cache_state_t *cache_state = user_data;
  cache_state->n_obj += 1;
  cache_state->used_size += cache_obj->obj_size;
  if (cache_obj->exp_time < cache_state->cur_time){
    cache_state->n_expired_obj += 1;
    cache_state->n_expired_byte += cache_obj->obj_size;
  }
}

static inline void get_cache_state(cache_t *cache, cache_state_t* cache_state){
//  g_hash_table_foreach(cache->core.hashtable, get_cache_state_ht_iter, cache_state);
  hashtable_foreach(cache->core.hashtable_new, get_cache_state_ht_iter, cache_state);
}




#ifdef __cplusplus
}
#endif

#endif //LIBMIMIRCACHE_CACHEUTILS_H
