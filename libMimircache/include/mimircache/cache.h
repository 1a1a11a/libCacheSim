//
//  cache.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef CACHE_H
#define CACHE_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <math.h>
#include <stdbool.h>
#include "reader.h"
#include "const.h"
#include "macro.h"
#include "logging.h"

#include "request.h"
#include "cacheObj.h"
#include "../config.h"
#include "../../dataStructure/include/hashtable.h"

struct cache;
typedef struct cache cache_t;

typedef struct {
  gint64 cache_size;
  gint64 default_ttl;
  int hash_power;
} common_cache_params_t;

typedef cache_t* (*cache_init_func_ptr)(common_cache_params_t, void *);
typedef void (*cache_free_func_ptr)(cache_t*);
typedef cache_check_result_t (*cache_get_func_ptr)(cache_t*, request_t *);
typedef cache_check_result_t (*cache_check_func_ptr)(cache_t*, request_t *, bool);
typedef void (*_cache_insert_func_ptr)(cache_t*, request_t *);
typedef void (*_cache_evict_func_ptr)(cache_t*, request_t *, cache_obj_t *);
typedef void (*cache_remove_obj_func_ptr)(cache_t*, void *);


struct cache_core {
  hashtable_t *hashtable_new;
  cache_obj_t* list_head;     // for LRU and FIFO
  cache_obj_t* list_tail;     // for LRU and FIFO

  cache_get_func_ptr get;
  cache_check_func_ptr check;
  _cache_insert_func_ptr _insert;
  _cache_evict_func_ptr _evict;
  cache_remove_obj_func_ptr remove_obj;
  cache_init_func_ptr cache_init;
  cache_free_func_ptr cache_free;

  guint64 req_cnt;
  gint64 cache_size;
  gint64 used_size;
  gint64 default_ttl;

  void *cache_specific_init_params;
  char cache_name[32];
};

typedef struct {
  uint64_t stored_obj_cnt;
  uint64_t used_bytes;
  uint64_t cache_size;
  uint64_t expired_obj_cnt;
  uint64_t expired_bytes;
  uint64_t cur_time;
} cache_state_t;


struct cache {
  void *cache_params;     // put it here so we can always get it within one cacheline
  struct cache_core core;
};


static inline void *_get_func_handle(char *func_name, const char *const cache_name, bool must_have, bool internal_func) {
  static void *handle = NULL;
  if (handle == NULL)
    handle = dlopen(NULL, RTLD_GLOBAL);

  char full_func_name[128];
  if (internal_func)
    sprintf(full_func_name, "_%s_%s", cache_name, func_name);
  else
    sprintf(full_func_name, "%s_%s", cache_name, func_name);

  void *func_ptr = dlsym(handle, full_func_name);

  if (must_have && func_ptr == NULL){
    ERROR("unable to find %s error %s\n", full_func_name, dlerror());
    abort();
  }
  return func_ptr;
}

static inline cache_t *cache_struct_init(const char *const cache_name, common_cache_params_t params) {
  cache_t *cache = my_malloc(cache_t);
  memset(cache, 0, sizeof(cache_t));
  strncpy(cache->core.cache_name, cache_name, 32);
  cache->core.cache_size = params.cache_size;
  cache->core.cache_specific_init_params = NULL;
  cache->core.default_ttl = params.default_ttl;
  int hash_power = HASH_POWER_DEFAULT;
  if (params.hash_power > 0 && params.hash_power < 40)
    hash_power = params.hash_power;
  cache->core.hashtable_new = create_hashtable(hash_power);
  hashtable_add_ptr_to_monitoring(cache->core.hashtable_new, &cache->core.list_head);
  hashtable_add_ptr_to_monitoring(cache->core.hashtable_new, &cache->core.list_tail);

  cache->core.cache_init = (cache_init_func_ptr) _get_func_handle("init", cache_name, true, false);
  cache->core.cache_free = (cache_free_func_ptr) _get_func_handle("free", cache_name, true, false);
  cache->core.get = (cache_get_func_ptr) _get_func_handle("get", cache_name, true, false);
  cache->core.check = (cache_check_func_ptr) _get_func_handle("check", cache_name, true, false);
  cache->core._insert = (_cache_insert_func_ptr) _get_func_handle("insert", cache_name, true, true);
  cache->core._evict = (_cache_evict_func_ptr) _get_func_handle("evict", cache_name, true, true);
  cache->core.remove_obj = (cache_remove_obj_func_ptr) _get_func_handle("remove_obj", cache_name, false, false);

  return cache;
}

static inline void cache_struct_free(cache_t *cache){
  free_hashtable(cache->core.hashtable_new);
  my_free(sizeof(cache_t), cache);
}


static inline cache_t *create_cache_with_new_size(cache_t *old_cache, gint64 new_size){
  common_cache_params_t cc_params = {.cache_size=new_size, .default_ttl=old_cache->core.default_ttl};
  cache_t *cache = old_cache->core.cache_init(cc_params, old_cache->core.cache_specific_init_params);
  return cache;
}

//static inline cache_check_result_t
//cache_check(cache_t *cache, request_t *req, bool update_cache, cache_obj_t **cache_obj_ret) {
//  cache_obj_t *cache_obj = hashtable_find(cache->core.hashtable_new, req);
//  if (cache_obj_ret != NULL) *cache_obj_ret = cache_obj;
//  if (cache_obj == NULL) {
//    return cache_miss_e;
//  }
//  if (likely(update_cache)) {
//    if (unlikely(cache_obj->obj_size != req->obj_size)) {
//      cache->core.used_size += (req->obj_size - cache_obj->obj_size);
//      cache_obj->obj_size = req->obj_size;
//    }
//  }
//  return cache_hit_e;
//}

static inline cache_check_result_t cache_check(cache_t *cache, request_t *req, bool update_cache, cache_obj_t **cache_obj_ret) {
  cache_obj_t *cache_obj = hashtable_find(cache->core.hashtable_new, req);
  if (cache_obj_ret != NULL) *cache_obj_ret = cache_obj;
  if (cache_obj == NULL) {
    return cache_miss_e;
  }

  cache_check_result_t ret = cache_hit_e;
#ifdef SUPPORT_TTL
  if (cache->core.default_ttl != 0) {
    if (cache_obj->exp_time < req->real_time) {
      ret = expired_e;
      if (likely(update_cache))
        cache_obj->exp_time = req->real_time + (req->ttl != 0?req->ttl:cache->core.default_ttl);
    }
  }
#endif
  if (likely(update_cache)) {
    if (unlikely(cache_obj->obj_size != req->obj_size)) {
      cache->core.used_size -= cache_obj->obj_size;
      cache->core.used_size += req->obj_size;
      cache_obj->obj_size = req->obj_size;
    }
  }
  return ret;
}

static inline cache_check_result_t cache_get(cache_t *cache, request_t *req) {
//  DEBUG("%llu req %llu size %lu cache size %llu\n",
//      ULL(cache->core.req_cnt), ULL(req->obj_id_int), UL(req->obj_size), ULL(cache->core.used_size));

  cache_check_result_t cache_check = cache->core.check(cache, req, true);
  if (req->obj_size <= cache->core.cache_size) {
    if (cache_check == cache_miss_e)
      cache->core._insert(cache, req);

    while (cache->core.used_size > cache->core.cache_size)
      cache->core._evict(cache, req, NULL);
  } else {
    WARNING("req %lld: obj size %ld larger than cache size %ld\n", (long long) cache->core.req_cnt,
            (long) req->obj_size, (long) cache->core.cache_size);
  }
  cache->core.req_cnt += 1;
  return cache_check;
}

/** update LRU list, used by all cache replacement algorithms that need to use LRU list **/
static inline cache_obj_t *cache_insert_LRU(cache_t* cache, request_t* req){
  cache->core.used_size += req->obj_size;
  cache_obj_t *cache_obj = hashtable_insert(cache->core.hashtable_new, req);
  if (unlikely(cache->core.list_head == NULL)) {
    // an empty list, this is the first insert
    cache->core.list_head = cache_obj;
    cache->core.list_tail = cache_obj;
  } else {
    cache->core.list_tail->list_next = cache_obj;
    cache_obj->list_prev = cache->core.list_tail;
  }
  cache->core.list_tail = cache_obj;
  return cache_obj;
}


#ifdef __cplusplus
}
#endif

#endif /* cache_h */
