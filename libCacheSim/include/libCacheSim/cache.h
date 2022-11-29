//
//  cache.h
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef CACHE_H
#define CACHE_H

#include <math.h>

#include "../config.h"
#include "admissionAlgo.h"
#include "cacheObj.h"
#include "const.h"
#include "logging.h"
#include "macro.h"
#include "request.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cache;
typedef struct cache cache_t;

typedef struct {
  uint64_t cache_size;
  uint64_t default_ttl;
  int32_t hashpower;
  bool consider_obj_metadata;
} common_cache_params_t;

typedef cache_t *(*cache_init_func_ptr)(const common_cache_params_t,
                                        const char *);

typedef void (*cache_free_func_ptr)(cache_t *);

typedef cache_ck_res_e (*cache_get_func_ptr)(cache_t *, const request_t *);

typedef cache_ck_res_e (*cache_check_func_ptr)(cache_t *, const request_t *,
                                               const bool);

typedef bool (*cache_can_insert_func_ptr)(cache_t *cache, const request_t *req);

typedef cache_obj_t *(*cache_insert_func_ptr)(cache_t *, const request_t *);

typedef void (*cache_evict_func_ptr)(cache_t *, const request_t *,
                                     cache_obj_t *);

typedef cache_obj_t *(*cache_to_evict_func_ptr)(cache_t *);

typedef void (*cache_remove_func_ptr)(cache_t *, const obj_id_t);

typedef int64_t (*cache_get_occupied_byte_func_ptr)(const cache_t *);

typedef int64_t (*cache_get_n_obj_func_ptr)(const cache_t *);

// #define EVICTION_AGE_ARRAY_SZE 40
#define EVICTION_AGE_ARRAY_SZE 320
#define EVICTION_AGE_LOG_BASE 1.08
#define CACHE_NAME_ARRAY_LEN 64
typedef struct {
  uint64_t n_warmup_req;
  uint64_t n_req;
  uint64_t n_req_byte;
  uint64_t n_miss;
  uint64_t n_miss_byte;

  uint64_t n_obj;
  uint64_t occupied_size;
  uint64_t cache_size;

  /* current trace time, used to determine obj expiration */
  uint64_t curr_rtime;
  uint64_t expired_obj_cnt;
  uint64_t expired_bytes;
  char cache_name[CACHE_NAME_ARRAY_LEN];
} cache_stat_t;

struct hashtable;
struct cache {
  struct hashtable *hashtable;

  cache_get_func_ptr get;
  cache_check_func_ptr check;
  cache_can_insert_func_ptr can_insert;
  cache_insert_func_ptr insert;
  cache_evict_func_ptr evict;
  cache_remove_func_ptr remove;
  cache_init_func_ptr cache_init;
  cache_free_func_ptr cache_free;
  cache_to_evict_func_ptr to_evict;
  cache_get_occupied_byte_func_ptr get_occupied_byte;
  cache_get_n_obj_func_ptr get_n_obj;

  admissioner_t *admissioner;

  cache_obj_t *q_head;  // for LRU and FIFO
  cache_obj_t *q_tail;  // for LRU and FIFO

  void *eviction_params;

  int64_t n_req; /* number of requests (used by some eviction algo) */
  uint64_t n_obj;
  uint64_t occupied_size;

  uint64_t cache_size;
  uint64_t default_ttl;
  int32_t per_obj_metadata_size;

  /* cache stat is not updated automatically, it is popped up only in
   * some situations */
  // cache_stat_t stat;
  char cache_name[CACHE_NAME_ARRAY_LEN];
  const char *init_params;
  
  /* not used by most algorithms */
  int32_t *future_stack_dist;
  int64_t future_stack_dist_array_size;

  int64_t log_eviction_age_cnt[EVICTION_AGE_ARRAY_SZE];
};

static inline common_cache_params_t default_common_cache_params(void) {
  common_cache_params_t params;
  params.cache_size = 1 * GiB;
  params.default_ttl = 364 * 86400;
  params.hashpower = 20;
  params.consider_obj_metadata = false;
  return params;
}

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
cache_t *create_cache_with_new_size(const cache_t *old_cache,
                                    const uint64_t new_size);

/**
 * a common cache check function
 * @param cache
 * @param req
 * @param update_cache
 * @param cache_obj_ret
 * @return
 */
cache_ck_res_e cache_check_base(cache_t *cache, const request_t *req,
                                const bool update_cache,
                                cache_obj_t **cache_obj_ret);

/**
 * a common cache get function
 * @param cache
 * @param req
 * @return
 */
cache_ck_res_e cache_get_base(cache_t *cache, const request_t *req);

/**
 * @brief check whether the object can be inserted into the cache
 *
 * @param cache
 * @param req
 * @return true
 * @return false
 */
bool cache_can_insert_default(cache_t *cache, const request_t *req);

/**
 * this function is called by all caches to
 * insert an object into the cache, update the hash table and cache metadata
 * @param cache
 * @param req
 * @return
 */
cache_obj_t *cache_insert_base(cache_t *cache, const request_t *req);

/**
 * insert used by LRU/FIFO, first call cache_insert_base then
 * update LRU, all algorithms using LRU list should call this
 * @param cache
 * @param req
 * @return
 */
cache_obj_t *cache_insert_LRU(cache_t *cache, const request_t *req);

/**
 * remove object from the cache, this function handles the cache metadata
 * and removing from hash table
 *
 * @param cache
 * @param obj
 */
void cache_remove_obj_base(cache_t *cache, cache_obj_t *obj);

/**
 * @brief evict an object using LRU algorithm
 *
 * @param cache
 * @param req
 * @param evicted_obj
 */
void cache_evict_LRU(cache_t *cache, const request_t *req,
                     cache_obj_t *evicted_obj);

/**
 * @brief get an object from the cache using request object id
 *
 * @param cache
 * @param req
 * @return cache_obj_t*
 */
cache_obj_t *cache_get_obj(cache_t *cache, const request_t *req);

/**
 * @brief get an object from the cache using object id
 *
 * @param cache
 * @param id
 * @return cache_obj_t*
 */
cache_obj_t *cache_get_obj_by_id(cache_t *cache, const obj_id_t id);

/**
 * @brief get the number of bytes occupied, this is the default
 * for most algorithms, but some algorithms may have different implementation
 * for example, SLRU and SFIFO
 *
 * @param cache
 */
static inline int64_t cache_get_occupied_byte_default(const cache_t *cache) {
  return cache->occupied_size;
}

/**
 * @brief get the number of objects in the cache, this is the default
 * for most algorithms, but some algorithms may have different implementation
 * for example, SLRU and SFIFO
 *
 * @param cache
 */
static inline int64_t cache_get_n_obj_default(const cache_t *cache) {
  return cache->n_obj;
}

/**
 * @brief print cache
 *
 * @param cache
 */
static inline void print_cache(cache_t *cache) {
  printf("%s cache size %" PRIu64 ", occupied size %" PRIu64 ", n_req %" PRIu64
         ", n_obj %" PRIu64 ", default TTL %" PRIu64
         ", per_obj_metadata_size %" PRIi32 "\n",
         cache->cache_name, cache->cache_size, cache->occupied_size,
         cache->n_req, cache->n_obj, cache->default_ttl,
         cache->per_obj_metadata_size);
}

/**
 * @brief record eviction age in wall clock time
 *
 * @param cache
 * @param age
 */
static inline void record_log2_eviction_age(cache_t *cache, const int age) {
#define LOG2(X) \
  ((unsigned)(8 * sizeof(unsigned long long) - __builtin_clzll((X))))

  int age_log2 = age == 0 ? 0 : LOG2(age);
  cache->log_eviction_age_cnt[age_log2] += 1;
#undef LOG2
}

static inline void record_eviction_age(cache_t *cache, const int64_t age) {
  double log_base = log(EVICTION_AGE_LOG_BASE);
  int age_log = age == 0 ? 0 : (int)ceil(log((double)age) / log_base);
  cache->log_eviction_age_cnt[age_log] += 1;
}

/**
 * @brief print the recorded eviction age
 *
 * @param cache
 */
void print_eviction_age(const cache_t *cache);

/**
 * @brief dump the eviction age to the file
 *
 * @param cache
 * @param ofilepath
 * @return whether the dump is successful
 */
bool dump_eviction_age(const cache_t *cache, const char *ofilepath);

/**
 * @brief dump the ages of the cached objects via forcing evictions
 *
 * @param cache
 * @param req used to provide the current time
 * @param ofilepath
 * @return whether the dump is successful
 */
bool dump_cached_obj_age(cache_t *cache, const request_t *req,
                         const char *ofilepath);

#ifdef __cplusplus
}
#endif

#endif /* cache_h */
