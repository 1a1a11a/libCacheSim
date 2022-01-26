//
//  cache.h
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef CACHE_H
#define CACHE_H

#include "../config.h"
#include "const.h"
#include "logging.h"
#include "macro.h"

#include "request.h"
#include "cacheObj.h"

#include <inttypes.h>
#include <glib.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


struct cache;
typedef struct cache cache_t;

typedef struct {
  uint64_t cache_size;
  uint64_t default_ttl;
  int32_t hashpower;
  int32_t per_obj_overhead;
} common_cache_params_t;

typedef cache_t *(*cache_init_func_ptr)(common_cache_params_t, void *);

typedef void (*cache_free_func_ptr)(cache_t *);

typedef cache_ck_res_e (*cache_get_func_ptr)(cache_t *, request_t *);

typedef cache_ck_res_e (*cache_check_func_ptr)(cache_t *, request_t *, bool);

typedef void (*cache_insert_func_ptr)(cache_t *, request_t *);

typedef void (*cache_evict_func_ptr)(cache_t *, request_t *, cache_obj_t *);

typedef void (*cache_remove_func_ptr)(cache_t *, obj_id_t);

typedef bool (*cache_admission_func_ptr)(cache_t *, request_t*);

#define MAX_EVICTION_AGE_ARRAY_SZE 64
typedef struct {
  uint64_t n_warmup_req;
  uint64_t n_req;
  uint64_t n_req_byte;
  uint64_t n_miss;
  uint64_t n_miss_byte;

  uint64_t n_obj;
  uint64_t occupied_size;
  uint64_t cache_size;

  /* eviction age in wall clock time */
  int log2_eviction_rage[MAX_EVICTION_AGE_ARRAY_SZE];
  /* eviction age in virtual time/num of requests */
  int log2_eviction_vage[MAX_EVICTION_AGE_ARRAY_SZE];

  uint64_t curr_rtime; /* current trace time, used to determine obj expiration */
  uint64_t expired_obj_cnt;
  uint64_t expired_bytes;
} cache_stat_t;


struct hashtable;
struct cache {
  struct hashtable *hashtable;
  cache_obj_t *q_head; // for LRU and FIFO
  cache_obj_t *q_tail; // for LRU and FIFO

  void *init_params;
  void *eviction_params;
  void *admission_params;

  cache_get_func_ptr get;
  cache_check_func_ptr check;
  cache_insert_func_ptr insert;
  cache_evict_func_ptr evict;
  cache_remove_func_ptr remove;
  cache_admission_func_ptr admit;
  cache_init_func_ptr cache_init;
  cache_free_func_ptr cache_free;

  int64_t n_req;  /* number of requests (used by some eviction algo) */
  uint64_t n_obj;
  uint64_t occupied_size;

  uint64_t cache_size;
  uint64_t default_ttl;
  int32_t per_obj_overhead;

  /* cache stat is not updated automatically, it is popped up only in
   * some situations */
  cache_stat_t stat;
  char cache_name[32];
};

/**
 * initialize the cache struct, must be called in all cache_init functions
 * @param cache_name
 * @param params
 * @return
 */
cache_t *cache_struct_init(const char *cache_name,
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
cache_t *create_cache_with_new_size(cache_t *old_cache, uint64_t new_size);

/**
 * a common cache check function
 * @param cache
 * @param req
 * @param update_cache
 * @param cache_obj_ret
 * @return
 */
cache_ck_res_e cache_check_base(cache_t *cache, request_t *req, bool update_cache,
                           cache_obj_t **cache_obj_ret);

/**
 * a common cache get function
 * @param cache
 * @param req
 * @return
 */
cache_ck_res_e cache_get_base(cache_t *cache, request_t *req);

/**
 * a common cache insert function
 * it updates LRU list, used by all algorithms that need to use LRU list
 * @param cache
 * @param req
 * @return
 */
cache_obj_t *cache_insert_base(cache_t *cache, request_t *req);


/**
 * insert for LRU/FIFO, first call cache_insert_base then
 * update LRU, used by all algorithms that need to use LRU list
 * @param cache
 * @param req
 * @return
 */
cache_obj_t *cache_insert_LRU(cache_t *cache, request_t *req);

/**
 * remove object from the cache, this function handles the cache metadata
 * and removing from hash table
 *
 * @param cache
 * @param obj
 */
void cache_remove_obj_base(cache_t *cache, cache_obj_t *obj);

void cache_evict_LRU(cache_t *cache,
                     request_t *req,
                     cache_obj_t *evicted_obj);

cache_obj_t *cache_get_obj(cache_t *cache, request_t *req);

cache_obj_t *cache_get_obj_by_id(cache_t *cache, obj_id_t id);


static inline void record_eviction_age(cache_t *cache, int age) {
#define LOG2(X) ((unsigned) (8*sizeof (unsigned long long) - __builtin_clzll((X))))

  if (age != 0)
    age = LOG2(age);
  cache->stat.log2_eviction_rage[age] += 1;
}

static inline void print_eviction_age(cache_t *cache) {
  printf("eviction age %d:%d, ", 0, cache->stat.log2_eviction_rage[0]);
  for (int i = 1; i < MAX_EVICTION_AGE_ARRAY_SZE; i++) {
    if (cache->stat.log2_eviction_rage[i] > 0) {
      if (cache->stat.log2_eviction_rage[i] > 1000000)
        printf("%d:%.1lfm, ", 1u << (i-1), (double) cache->stat.log2_eviction_rage[i]/1000000);
      else if (cache->stat.log2_eviction_rage[i] > 1000)
        printf("%d:%.1lfk, ", 1u << (i-1), (double) cache->stat.log2_eviction_rage[i]/1000);
    }
  }
  printf("\n");
}

#ifdef __cplusplus
}
#endif

#endif /* cache_h */
