/**
 * @file cache.h
 * @brief This file contains the core data structures and functions for the cache simulator.
 *
 * It defines the main cache structure `cache_t` and the function pointers that allow for
 * different cache eviction, admission, and prefetching policies to be plugged in.
 * It also provides base functions for common cache operations.
 */

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
#include "prefetchAlgo.h"
#include "request.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cache;
/**
 * @brief The main cache structure.
 *
 * This structure holds all the information about a cache, including its size,
 * statistics, and pointers to the functions that implement the cache logic.
 */
typedef struct cache cache_t;

/**
 * @brief Common parameters for initializing a cache.
 */
typedef struct {
  uint64_t cache_size;          /**< The size of the cache in bytes. */
  uint64_t default_ttl;         /**< The default time-to-live for cache objects in seconds. */
  int32_t hashpower;            /**< The hash power for the internal hash table (size = 2^hashpower). */
  bool consider_obj_metadata;   /**< Whether to consider object metadata size in cache size calculation. */
} common_cache_params_t;

/** @brief Function pointer for initializing a cache. */
typedef cache_t *(*cache_init_func_ptr)(const common_cache_params_t, const char *);

/** @brief Function pointer for freeing a cache. */
typedef void (*cache_free_func_ptr)(cache_t *);

/** @brief Function pointer for processing a get request. Returns true if the object is in the cache. */
typedef bool (*cache_get_func_ptr)(cache_t *, const request_t *);

/** @brief Function pointer for finding an object in the cache. */
typedef cache_obj_t *(*cache_find_func_ptr)(cache_t *, const request_t *, const bool);

/** @brief Function pointer to check if an object can be inserted into the cache. */
typedef bool (*cache_can_insert_func_ptr)(cache_t *cache, const request_t *req);

/** @brief Function pointer for inserting an object into the cache. */
typedef cache_obj_t *(*cache_insert_func_ptr)(cache_t *, const request_t *);

/** @brief Function pointer to check if eviction is needed before inserting a new object. */
typedef bool (*cache_need_eviction_func_ptr)(cache_t *, const request_t *);

/** @brief Function pointer for evicting an object from the cache. */
typedef void (*cache_evict_func_ptr)(cache_t *, const request_t *);

/** @brief Function pointer for selecting an object to evict. */
typedef cache_obj_t *(*cache_to_evict_func_ptr)(cache_t *, const request_t *);

/** @brief Function pointer for removing an object from the cache by its ID. */
typedef bool (*cache_remove_func_ptr)(cache_t *, const obj_id_t);

/** @brief Function pointer for removing a specific cache object. */
typedef void (*cache_remove_obj_func_ptr)(cache_t *, cache_obj_t *obj);

/** @brief Function pointer for getting the number of occupied bytes in the cache. */
typedef int64_t (*cache_get_occupied_byte_func_ptr)(const cache_t *);

/** @brief Function pointer for getting the number of objects in the cache. */
typedef int64_t (*cache_get_n_obj_func_ptr)(const cache_t *);

/** @brief Function pointer for printing the cache state for debugging. */
typedef void (*cache_print_cache_func_ptr)(const cache_t *);

#define EVICTION_AGE_ARRAY_SZE 320
#define EVICTION_AGE_LOG_BASE 1.08
#define CACHE_NAME_ARRAY_LEN 64
#define CACHE_INIT_PARAMS_LEN 256

/**
 * @brief Statistics for a cache.
 */
typedef struct {
  int64_t n_warmup_req;         /**< Number of warmup requests. */
  int64_t n_req;                /**< Number of requests processed. */
  int64_t n_req_byte;           /**< Total bytes of requests processed. */
  int64_t n_miss;               /**< Number of cache misses. */
  int64_t n_miss_byte;          /**< Total bytes of cache misses. */

  int64_t n_obj;                /**< Number of objects in the cache. */
  int64_t occupied_byte;        /**< Total bytes occupied by objects in the cache. */
  int64_t cache_size;           /**< The size of the cache in bytes. */
  float sampler_ratio;          /**< The sampling ratio if sampling is used. */
  int64_t curr_rtime;           /**< Current trace time, used for object expiration. */
  int64_t expired_obj_cnt;      /**< Number of objects expired from the cache. */
  int64_t expired_bytes;        /**< Total bytes of objects expired from the cache. */

  char cache_name[CACHE_NAME_ARRAY_LEN]; /**< The name of the cache. */
} cache_stat_t;

struct hashtable;

/**
 * @brief The main cache structure.
 */
struct cache {
  struct hashtable *hashtable;  /**< The hash table for object lookup. */

  // Core cache operations implemented via function pointers
  cache_init_func_ptr cache_init;       /**< Function to initialize the cache. */
  cache_free_func_ptr cache_free;       /**< Function to free the cache. */
  cache_get_func_ptr get;               /**< Function to process a get request. */
  cache_find_func_ptr find;             /**< Function to find an object. */
  cache_can_insert_func_ptr can_insert; /**< Function to check if an object can be inserted. */
  cache_insert_func_ptr insert;         /**< Function to insert an object. */
  cache_need_eviction_func_ptr need_eviction; /**< Function to check if eviction is needed. */
  cache_evict_func_ptr evict;           /**< Function to evict an object. */
  cache_remove_func_ptr remove;         /**< Function to remove an object by ID. */
  cache_to_evict_func_ptr to_evict;     /**< Function to select an object for eviction. */
  cache_get_occupied_byte_func_ptr get_occupied_byte; /**< Function to get occupied bytes. */
  cache_get_n_obj_func_ptr get_n_obj;   /**< Function to get the number of objects. */
  cache_print_cache_func_ptr print_cache; /**< Function to print cache state. */

  admissioner_t *admissioner;   /**< The admission policy. */
  struct prefetcher *prefetcher; /**< The prefetching policy. */
  void *eviction_params;        /**< Parameters for the eviction policy. */

  int64_t n_req;                /**< A counter for requests, used as logical time by some algorithms. */

  /**************** private fields *****************/
  int64_t n_obj;                /**< (Private) Number of objects. Use get_n_obj() instead. */
  int64_t occupied_byte;        /**< (Private) Occupied bytes. Use get_occupied_byte() instead. */
  /************ end of private fields *************/

  cache_obj_t *to_evict_candidate; /**< Candidate object for eviction. */
  int64_t to_evict_candidate_gen_vtime; /**< Generation time of the eviction candidate. */

  // Const properties
  int64_t cache_size;           /**< The size of the cache in bytes. */
  int64_t default_ttl;          /**< Default time-to-live for objects. */
  int32_t obj_md_size;          /**< Size of metadata per object. */

  char cache_name[CACHE_NAME_ARRAY_LEN]; /**< Name of the cache algorithm. */
  char init_params[CACHE_INIT_PARAMS_LEN]; /**< Initialization parameters string. */

  const char *last_request_metadata; /**< Metadata from the last request. */
#if defined(TRACK_EVICTION_V_AGE)
  bool track_eviction_age;
#endif
#if defined(TRACK_DEMOTION)
  bool track_demotion;
#endif

  /* not used by most algorithms */
  int32_t *future_stack_dist;
  int64_t future_stack_dist_array_size;

  int64_t log_eviction_age_cnt[EVICTION_AGE_ARRAY_SZE]; /**< Array to track eviction ages. */
};

/**
 * @brief Provides default parameters for a cache.
 * @return A `common_cache_params_t` struct with default values.
 */
static inline common_cache_params_t default_common_cache_params(void) {
  common_cache_params_t params;
  params.cache_size = 1 * GiB;
  params.default_ttl = (uint64_t)(364 * 86400);
  params.hashpower = 20;
  params.consider_obj_metadata = false;
  return params;
}

/**
 * @brief Initializes the base cache structure. This must be called by all cache_init functions.
 * @param cache_name The name of the cache algorithm.
 * @param params The common cache parameters.
 * @param init_params A pointer to the specific initialization parameters for the algorithm.
 * @return A pointer to the initialized cache structure.
 */
cache_t *cache_struct_init(const char *cache_name, common_cache_params_t params,
                           const void *const init_params);

/**
 * @brief Frees the base cache structure. This must be called by all cache_free functions.
 * @param cache A pointer to the cache structure to free.
 */
void cache_struct_free(cache_t *cache);

/**
 * @brief Creates a new cache with the same size and parameters as an existing one.
 * @param old_cache A pointer to the cache to clone.
 * @return A pointer to the newly created cache.
 */
cache_t *clone_cache(const cache_t *old_cache);

/**
 * @brief Creates a new cache with a different size but otherwise the same parameters.
 * @param old_cache A pointer to the cache to base the new one on.
 * @param new_size The new size for the cache in bytes.
 * @return A pointer to the newly created cache.
 */
cache_t *create_cache_with_new_size(const cache_t *old_cache,
                                    const uint64_t new_size);

/**
 * @brief A base function to find an object in the cache's hash table.
 * @param cache The cache to search in.
 * @param req The request containing the object ID to find.
 * @param update_cache Whether to update cache metadata upon finding the object (e.g., for LRU).
 * @return A pointer to the found cache object, or NULL if not found.
 */
cache_obj_t *cache_find_base(cache_t *cache, const request_t *req,
                             const bool update_cache);

/**
 * @brief A base 'get' function that handles finding and inserting an object.
 * @param cache The cache to operate on.
 * @param req The request to process.
 * @return True if the object was found in the cache (a hit), false otherwise (a miss).
 */
bool cache_get_base(cache_t *cache, const request_t *req);

/**
 * @brief Default function to check if an object can be inserted.
 * @param cache The cache.
 * @param req The request containing the object to insert.
 * @return True if the object is smaller than the cache size, false otherwise.
 */
bool cache_can_insert_default(cache_t *cache, const request_t *req);

/**
 * @brief A base function to insert an object into the cache.
 *
 * This function handles updating the hash table and cache metadata.
 * @param cache The cache to insert into.
 * @param req The request containing the object to insert.
 * @return A pointer to the newly created cache object.
 */
cache_obj_t *cache_insert_base(cache_t *cache, const request_t *req);

/**
 * @brief A base function to remove an object from the cache.
 *
 * This function updates cache metadata and optionally removes the object from the hash table.
 * It should be called at the end of eviction logic as it frees the object structure.
 * @param cache The cache.
 * @param obj The object to remove.
 * @param remove_from_hashtable If true, the object is also removed from the hash table.
 */
void cache_remove_obj_base(cache_t *cache, cache_obj_t *obj,
                           bool remove_from_hashtable);

/**
 * @brief A base function to evict an object from the cache.
 *
 * This is a wrapper around `cache_remove_obj_base` and is intended to be called
 * from eviction implementations.
 * @param cache The cache.
 * @param obj The object to evict.
 * @param remove_from_hashtable If true, the object is also removed from the hash table.
 */
void cache_evict_base(cache_t *cache, cache_obj_t *obj,
                      bool remove_from_hashtable);

/**
 * @brief Default function to get the number of occupied bytes in the cache.
 * @param cache The cache.
 * @return The number of occupied bytes.
 */
static inline int64_t cache_get_occupied_byte_default(const cache_t *cache) {
  return cache->occupied_byte;
}

/**
 * @brief Default function to get the number of objects in the cache.
 * @param cache The cache.
 * @return The number of objects.
 */
static inline int64_t cache_get_n_obj_default(const cache_t *cache) {
  return cache->n_obj;
}

/**
 * @brief Gets the reference time, which is the number of requests processed.
 * @param cache The cache.
 * @return The reference time.
 */
static inline int64_t cache_get_reference_time(const cache_t *cache) {
  return cache->n_req;
}

/**
 * @brief Gets the logical time, which is the number of requests processed.
 * @param cache The cache.
 * @return The logical time.
 */
static inline int64_t cache_get_logical_time(const cache_t *cache) {
  return cache->n_req;
}

/**
 * @brief Gets the virtual time, which is the number of requests processed.
 * @param cache The cache.
 * @return The virtual time.
 */
static inline int64_t cache_get_virtual_time(const cache_t *cache) {
  return cache->n_req;
}

/**
 * @brief Prints statistics about the cache.
 * @param cache The cache.
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
 * @brief Records the eviction age of an object using a log2 scale.
 * @param cache The cache.
 * @param age The age of the evicted object.
 */
static inline void record_log2_eviction_age(cache_t *cache,
                                            const unsigned long long age) {
  int age_log2 = age == 0 ? 0 : LOG2_ULL(age);
  cache->log_eviction_age_cnt[age_log2] += 1;
}

/**
 * @brief Records the eviction age of an object using a custom log base.
 * @param cache The cache.
 * @param obj The evicted object.
 * @param age The age of the evicted object.
 */
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
 * @brief Prints the recorded eviction age distribution to the console.
 * @param cache The cache.
 */
void print_eviction_age(const cache_t *cache);

/**
 * @brief Dumps the recorded eviction age distribution to a file.
 * @param cache The cache.
 * @param ofilepath The path to the output file.
 * @return True if the dump was successful, false otherwise.
 */
bool dump_eviction_age(const cache_t *cache, const char *ofilepath);

/**
 * @brief Dumps the ages of all currently cached objects by forcing eviction.
 * @param cache The cache.
 * @param req The current request, used to provide the current time.
 * @param ofilepath The path to the output file.
 * @return True if the dump was successful, false otherwise.
 */
bool dump_cached_obj_age(cache_t *cache, const request_t *req,
                         const char *ofilepath);

/**
 * @brief Generates a detailed name for the cache based on its configuration.
 * @param cache The cache.
 * @param str_dest The destination buffer for the name.
 * @param str_dest_len The length of the destination buffer.
 */
void generate_cache_name(cache_t *cache, char *str_dest, int str_dest_len);

#ifdef __cplusplus
}
#endif

#endif /* CACHE_H */
