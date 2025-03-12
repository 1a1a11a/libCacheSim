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
#include <string.h>

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

struct prefetcher;
typedef struct prefetcher prefetcher_t;

typedef struct {
  uint64_t cache_size;
  uint64_t default_ttl;
  int32_t hashpower;
  bool consider_obj_metadata;
} common_cache_params_t;

typedef cache_t *(*cache_init_func_ptr)(const common_cache_params_t,
                                        const char *);

typedef void (*cache_free_func_ptr)(cache_t *);

typedef bool (*cache_get_func_ptr)(cache_t *, const request_t *);

typedef cache_obj_t *(*cache_find_func_ptr)(cache_t *, const request_t *,
                                            const bool);

typedef bool (*cache_can_insert_func_ptr)(cache_t *cache, const request_t *req);

typedef cache_obj_t *(*cache_insert_func_ptr)(cache_t *, const request_t *);

typedef bool (*cache_need_eviction_func_ptr)(cache_t *, const request_t *);

typedef void (*cache_evict_func_ptr)(cache_t *, const request_t *);

typedef cache_obj_t *(*cache_to_evict_func_ptr)(cache_t *, const request_t *);

typedef bool (*cache_remove_func_ptr)(cache_t *, const obj_id_t);

typedef void (*cache_remove_obj_func_ptr)(cache_t *, cache_obj_t *obj);

typedef int64_t (*cache_get_occupied_byte_func_ptr)(const cache_t *);

typedef int64_t (*cache_get_n_obj_func_ptr)(const cache_t *);

typedef void (*cache_print_cache_func_ptr)(const cache_t *);

// #define EVICTION_AGE_ARRAY_SZE 40
#define EVICTION_AGE_ARRAY_SZE 320
#define EVICTION_AGE_LOG_BASE 1.08
#define CACHE_NAME_ARRAY_LEN 64
#define CACHE_STAT_NAME_ARRAY_LEN 640
#define CACHE_INIT_PARAMS_LEN 256
typedef struct {
  int64_t n_warmup_req;
  int64_t n_req;
  int64_t n_req_byte;
  int64_t n_miss;
  int64_t n_miss_byte;

  int64_t n_obj;
  int64_t occupied_byte;
  int64_t cache_size;
  float sampler_ratio;
  /* current trace time, used to determine obj expiration */
  int64_t curr_rtime;
  int64_t expired_obj_cnt;
  int64_t expired_bytes;

  char cache_name[CACHE_STAT_NAME_ARRAY_LEN];
} cache_stat_t;

struct hashtable;
struct cache {
  struct hashtable *hashtable;

  cache_init_func_ptr cache_init;
  cache_free_func_ptr cache_free;
  cache_get_func_ptr get;

  cache_find_func_ptr find;
  cache_can_insert_func_ptr can_insert;
  cache_insert_func_ptr insert;
  cache_need_eviction_func_ptr need_eviction;
  cache_evict_func_ptr evict;
  cache_remove_func_ptr remove;
  cache_to_evict_func_ptr to_evict;
  cache_get_occupied_byte_func_ptr get_occupied_byte;
  cache_get_n_obj_func_ptr get_n_obj;
  cache_print_cache_func_ptr print_cache;

  admissioner_t *admissioner;

  prefetcher_t *prefetcher;

  void *eviction_params;

  // other name: logical_time, virtual_time, reference_count
  int64_t n_req; /* number of requests (used by some eviction algo) */

  /**************** private fields *****************/
  // use cache->get_n_obj to obtain the number of objects in the cache
  // do not use this variable directly
  int64_t n_obj;
  // use cache->get_occupied_byte to obtain the number of objects in the cache
  // do not use this variable directly
  int64_t occupied_byte;
  /************ end of private fields *************/

  // because some algorithms choose different candidates
  // each time we want to evict, but we want to make sure
  // that the object returned from to_evict will be evicted
  // the next time eviction is called, so we record here
  cache_obj_t *to_evict_candidate;
  // we keep track when the candidate was generated, so that
  // old candidate is not used
  int64_t to_evict_candidate_gen_vtime;

  // const
  int64_t cache_size;
  int64_t default_ttl;
  int32_t obj_md_size;

  /* cache stat is not updated automatically, it is popped up only in
   * some situations */
  // cache_stat_t stat;
  char cache_name[CACHE_NAME_ARRAY_LEN];
  char init_params[CACHE_INIT_PARAMS_LEN];

  void *last_request_metadata;
#if defined(TRACK_EVICTION_V_AGE)
  bool track_eviction_age;
#endif
#if defined(TRACK_DEMOTION)
  bool track_demotion;
#endif

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
cache_t *cache_struct_init(const char *cache_name, common_cache_params_t params,
                           const void *const init_params);

/**
 * free the cache struct, must be called in all cache_free functions
 * @param cache
 */
void cache_struct_free(cache_t *cache);

/**
 * @brief create a new cache with the same size and parameters
 *
 * @param old_cache
 * @return cache_t*
 */
cache_t *clone_cache(const cache_t *old_cache);

/**
 * create a cache with new size
 * @param old_cache
 * @param new_size
 * @return
 */
cache_t *create_cache_with_new_size(const cache_t *old_cache,
                                    const uint64_t new_size);

/**
 * a function that finds object from the cache, it is used by
 * all eviction algorithms that directly use the hashtable
 *
 * @param cache
 * @param req
 * @param update_cache
 * @return
 */
cache_obj_t *cache_find_base(cache_t *cache, const request_t *req,
                             const bool update_cache);

/**
 * a common cache get function
 * @param cache
 * @param req
 * @return
 */
bool cache_get_base(cache_t *cache, const request_t *req);

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
 * @brief this function is called by all eviction algorithms that
 * need to remove an object from the cache, it updates the cache metadata,
 * because it frees the object struct, it needs to be called at the end of
 * the eviction function.
 *
 * @param cache the cache
 * @param obj the object to be removed
 */
void cache_remove_obj_base(cache_t *cache, cache_obj_t *obj,
                           bool remove_from_hashtable);

/**
 * @brief this function is called by all eviction algorithms in the eviction
 * function, it updates the cache metadata. Because it frees the object struct,
 * it needs to be called at the end of the eviction function.
 *
 * @param cache the cache
 * @param obj the object to be removed
 */
void cache_evict_base(cache_t *cache, cache_obj_t *obj,
                      bool remove_from_hashtable);

/**
 * @brief get the number of bytes occupied, this is the default
 * for most algorithms, but some algorithms may have different implementation
 * for example, SLRU and SFIFO
 *
 * @param cache
 */
static inline int64_t cache_get_occupied_byte_default(const cache_t *cache) {
  return cache->occupied_byte;
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

static inline int64_t cache_get_reference_time(const cache_t *cache) {
  return cache->n_req;
}

static inline int64_t cache_get_logical_time(const cache_t *cache) {
  return cache->n_req;
}

static inline int64_t cache_get_virtual_time(const cache_t *cache) {
  return cache->n_req;
}

/**
 * @brief print cache stat
 *
 * @param cache
 */
static inline void print_cache_stat(const cache_t *cache) {
  printf(
      "%s cache size %ld, occupied size %ld, n_req %ld, n_obj %ld, default TTL "
      "%ld, per_obj_metadata_size %d\n",
      cache->cache_name, (long)cache->cache_size,
      (long)cache->get_occupied_byte(cache), (long)cache->n_req,
      (long)cache->get_n_obj(cache), (long)cache->default_ttl,
      (int)cache->obj_md_size);
}

/**
 * @brief record eviction age in wall clock time
 *
 * @param cache
 * @param age
 */
static inline void record_log2_eviction_age(cache_t *cache,
                                            const unsigned long long age) {
  int age_log2 = age == 0 ? 0 : LOG2_ULL(age);
  cache->log_eviction_age_cnt[age_log2] += 1;
}

static inline void record_eviction_age(cache_t *cache, cache_obj_t *obj,
                                       const int64_t age) {
#if defined(TRACK_EVICTION_V_AGE)
  // note that the frequency is not correct for QDLP and Clock
  if (obj->obj_id % 101 == 0) {
    printf("%ld: %lu %ld %d\n", cache->n_req, obj->obj_id, age, obj->misc.freq);
  }
#endif

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

static inline void generate_cache_name(cache_t *cache ,char *str_dest) {
  char admis_name[CACHE_NAME_ARRAY_LEN] = "";
  char admis_param[CACHE_INIT_PARAMS_LEN] = "";
  if (cache->admissioner) {
    strncpy(admis_name, cache->admissioner->admissioner_name, CACHE_NAME_ARRAY_LEN);
    if (cache->admissioner->init_params) {
      strncpy(admis_param, (const char *)cache->admissioner->init_params, CACHE_INIT_PARAMS_LEN);
    }
  } 

  snprintf(str_dest, CACHE_STAT_NAME_ARRAY_LEN, 
    "evict:%s[%s] admit:%s[%s]",
    cache->cache_name, 
    cache->init_params, 
    admis_name, 
    admis_param
  );
}
                        
#ifdef __cplusplus
}
#endif

#endif /* cache_h */
