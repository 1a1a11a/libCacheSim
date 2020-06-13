//
//  a LRU module that supports different obj size
//
//
//  LRU.c
//  libMimircache
//
//  Created by Juncheng on 12/4/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//


#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include "LRU.h"
#include "../include/mimircache/macro.h"

/** current implementation of hash table does NOT allow the algorithm to hold any pointer to cache_obj,
  * because the object's stored location will change in hash table delete and expand!!!
  *
  *
  *
 **/


cache_t *LRU_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
  cache_t *cache = cache_struct_init("LRU", ccache_params);
  return cache;
}

void LRU_free(cache_t *cache) {
  cache_struct_free(cache);
}

cache_check_result_t LRU_check(cache_t *cache, request_t *req, bool update_cache) {
  cache_obj_t *cache_obj;
  cache_check_result_t ret = cache_check(cache, req, update_cache, &cache_obj);

  if (cache_obj && likely(update_cache)) {
    /* lru_tail is the newest, move cur obj to lru_tail */
    move_obj_to_tail(&cache->core.list_head, &cache->core.list_tail, cache_obj);
  }
  return ret;
}


cache_check_result_t LRU_get(cache_t *cache, request_t *req) {
  return cache_get(cache, req);
}

void _LRU_insert(cache_t *cache, request_t *req) {
  cache_insert_LRU(cache, req);
}


void _LRU_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  // currently not handle the case when all objects are evicted
  cache_obj_t *obj_to_evict = cache->core.list_head;
  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }
  DEBUG_ASSERT(cache->core.list_head != cache->core.list_head->list_next);
  cache->core.list_head = cache->core.list_head->list_next;
  cache->core.list_head->list_prev = NULL;
  DEBUG_ASSERT(cache->core.used_size >= obj_to_evict->obj_size);
  cache->core.used_size -= obj_to_evict->obj_size;
  hashtable_delete(cache->core.hashtable_new, obj_to_evict);
//  DEBUG_ASSERT(cache->core.list_head != cache->core.list_head->list_next);
  /** obj_to_evict is not freed or returned to hashtable, if you have extra_metadata allocated with obj_to_evict,
   * you need to free them now, otherwise, there will be memory leakage **/
}


void LRU_remove_obj(cache_t *cache, request_t *req) {
  cache_obj_t *cache_obj = hashtable_find(cache->core.hashtable_new, req);
  if (cache_obj == NULL) {
    WARNING("obj to remove is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->core.list_head, &cache->core.list_tail, cache_obj);
  hashtable_delete(cache->core.hashtable_new, cache_obj);

  assert (cache->core.used_size >= cache_obj->obj_size);
  cache->core.used_size -= cache_obj->obj_size;
}


///**************************** TTL support **************************/
//cache_check_result_t LRU_check_with_ttl(cache_t *cache, request_t *req, bool update_cache) {
//  LRU_params_t *LRU_params = (LRU_params_t *) (cache->cache_params);
//  cache_obj_t *cache_obj = hashtable_find(cache->core.hashtable_new, req);
//  if (cache_obj == NULL)
//    return cache_miss_e;
//
//  if (unlikely(!update_cache)) {
//    return cache_obj->exp_time < req->real_time ? expired_e : cache_hit_e;
//  }
//
//  cache_check_result_t ret = cache_hit_e;
//  if (cache_obj->exp_time < req->real_time) {
//    cache_obj->exp_time = req->real_time + req->ttl;
//    ret = expired_e;
//  }
//  if (cache_obj->obj_size != req->obj_size) {
//    cache->core.used_size += (req->obj_size - cache_obj->obj_size);
//    cache_obj->obj_size = req->obj_size;
//  }
//  move_obj_to_tail(&LRU_params->lru_head, &LRU_params->lru_tail, cache_obj);
//  return ret;
//}
//
//cache_check_result_t LRU_get_with_ttl(cache_t *cache, request_t *req) {
//  if (req->ttl == 0)
//    req->ttl = cache->core.default_ttl;
//
//  cache_check_result_t cache_check = LRU_check_with_ttl(cache, req, true);
//
//  if (req->obj_size <= cache->core.cache_size) {
//    if (cache_check == cache_miss_e)
//      _LRU_insert(cache, req);
//    while (cache->core.used_size > cache->core.cache_size)
//      _LRU_evict(cache, req, NULL);
//  } else {
//    WARNING("req %lld: obj size %ld larger than cache size %ld, will not insert/update\n",
//            (long long) cache->core.req_cnt, (long) req->obj_size, (long) cache->core.cache_size);
//  }
//  cache->core.req_cnt += 1;
//  return cache_check;
//}


#ifdef __cplusplus
extern "C" {
#endif
