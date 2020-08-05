//
//  cache.h
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef CACHE_H
#define CACHE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config.h"
#include "cacheObj.h"
#include "const.h"
#include "logging.h"
#include "macro.h"
#include "reader.h"
#include "request.h"

#include "inttypes.h"
#include <dlfcn.h>
#include <glib.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cache;
typedef struct cache cache_t;

typedef struct {
  gint64 cache_size;
  gint64 default_ttl;
  int hash_power;
} common_cache_params_t;

typedef cache_t *(*cache_init_func_ptr)(common_cache_params_t, void *);

typedef void (*cache_free_func_ptr)(cache_t *);

typedef cache_ck_res_e (*cache_get_func_ptr)(cache_t *, request_t *);

typedef cache_ck_res_e (*cache_check_func_ptr)(cache_t *, request_t *, bool);

typedef void (*cache_insert_func_ptr)(cache_t *, request_t *);

typedef void (*cache_evict_func_ptr)(cache_t *, request_t *, cache_obj_t *);

typedef void (*cache_remove_obj_func_ptr)(cache_t *, cache_obj_t *);

typedef struct {
  uint64_t stored_obj_cnt;
  uint64_t used_bytes;
  uint64_t cache_size;
  uint64_t expired_obj_cnt;
  uint64_t expired_bytes;
  uint64_t cur_time;
} cache_state_t;

struct hashtable;
struct cache {
  struct hashtable *hashtable;
  cache_obj_t *list_head; // for LRU and FIFO
  cache_obj_t *list_tail; // for LRU and FIFO

  void *cache_params;

  cache_get_func_ptr get;
  cache_check_func_ptr check;
  cache_insert_func_ptr insert;
  cache_evict_func_ptr evict;
  cache_remove_obj_func_ptr remove_obj;
  cache_init_func_ptr cache_init;
  cache_free_func_ptr cache_free;

  uint64_t req_cnt;
  uint64_t cache_size;
  uint64_t occupied_size;
  uint64_t default_ttl;

  void *init_params;
  char cache_name[32];
};

/**
 * initialize the cache struct, must be called in all cache_init functions
 * @param cache_name
 * @param params
 * @return
 */
cache_t *cache_struct_init(const char *const cache_name,
                           common_cache_params_t params);

/**
 * free the cache struct, must be called in all cache_free functions
 * @param cache
 */
void cache_struct_free(cache_t *cache);

/**
 * create a cache with new size
 * @param old_cache
 * @param new_size
 * @return
 */
cache_t *create_cache_with_new_size(cache_t *old_cache, gint64 new_size);

/**
 * a common cache check function, currently used by LRU and FIFO
 * @param cache
 * @param req
 * @param update_cache
 * @param cache_obj_ret
 * @return
 */
cache_ck_res_e cache_check(cache_t *cache, request_t *req, bool update_cache,
                           cache_obj_t **cache_obj_ret);

/**
 * a common cache get function, currently used by LRU and FIFO
 * @param cache
 * @param req
 * @return
 */
cache_ck_res_e cache_get(cache_t *cache, request_t *req);

/**
 * a common cache insert function, currently used by LRU and FIFO
 * it updates LRU list, used by all algorithms that need to use LRU list
 * @param cache
 * @param req
 * @return
 */
cache_obj_t *cache_insert_LRU(cache_t *cache, request_t *req);


void cache_evict_LRU(cache_t *cache, request_t *req, cache_obj_t *evicted_obj);

cache_obj_t *cache_get_obj(cache_t *cache, request_t *req);


#ifdef __cplusplus
}
#endif

#endif /* cache_h */
