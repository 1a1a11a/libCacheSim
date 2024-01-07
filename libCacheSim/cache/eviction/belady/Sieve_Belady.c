

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/cache.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USE_BELADY_INSERTION
// eviction must be on
#define USE_BELADY_EVICTION

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  cache_obj_t *pointer;

  int64_t n_miss;
} Sieve_Belady_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void Sieve_Belady_free(cache_t *cache);
static bool Sieve_Belady_get(cache_t *cache, const request_t *req);
static cache_obj_t *Sieve_Belady_find(cache_t *cache, const request_t *req,
                                      const bool update_cache);
static cache_obj_t *Sieve_Belady_insert(cache_t *cache, const request_t *req);
static void Sieve_Belady_evict(cache_t *cache, const request_t *req);
static bool Sieve_Belady_remove(cache_t *cache, const obj_id_t obj_id);

static void Sieve_Belady_verify(cache_t *cache);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *Sieve_Belady_init(const common_cache_params_t ccache_params,
                           const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("Sieve_Belady", ccache_params, cache_specific_params);
  cache->cache_init = Sieve_Belady_init;
  cache->cache_free = Sieve_Belady_free;
  cache->get = Sieve_Belady_get;
  cache->find = Sieve_Belady_find;
  cache->insert = Sieve_Belady_insert;
  cache->evict = Sieve_Belady_evict;
  cache->remove = Sieve_Belady_remove;
  cache->to_evict = NULL;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 1;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc(Sieve_Belady_params_t);
  memset(cache->eviction_params, 0, sizeof(Sieve_Belady_params_t));
  Sieve_Belady_params_t *params =
      (Sieve_Belady_params_t *)cache->eviction_params;
  params->pointer = NULL;
  params->q_head = NULL;
  params->q_tail = NULL;
  params->n_miss = 0;

#ifdef USE_BELADY_INSERTION
  snprintf(cache->cache_name + strlen(cache->cache_name), CACHE_NAME_ARRAY_LEN,
           "%s", "_in");
#endif
#ifdef USE_BELADY_EVICTION
  snprintf(cache->cache_name + strlen(cache->cache_name), CACHE_NAME_ARRAY_LEN,
           "%s", "_ev");
#endif

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void Sieve_Belady_free(cache_t *cache) {
  free(cache->eviction_params);
  cache_struct_free(cache);
}

/**
 * @brief this function is the user facing API
 * it performs the following logic
 *
 * ```
 * if obj in cache:
 *    update_metadata
 *    return true
 * else:
 *    if cache does not have enough space:
 *        evict until it has space to insert
 *    insert the object
 *    return false
 * ```
 *
 * @param cache
 * @param req
 * @return true if cache hit, false if cache miss
 */

static bool Sieve_Belady_get(cache_t *cache, const request_t *req) {
  Sieve_Belady_params_t *params =
      (Sieve_Belady_params_t *)cache->eviction_params;

  bool ck_hit = cache_get_base(cache, req);
  if (!ck_hit) {
    params->n_miss++;
  }


  // if (cache->n_req % 100000 == 0) {
  //   // printf("sieve %ld %ld %lf\n", cache->cache_size, cache->n_req,
  //   //        (double)params->n_new_obj / (double)cache->cache_size);
  //   // printf("miss ratio %lf\n", (double)params->n_miss /
  //   (double)cache->n_req);
  // }

  return ck_hit;
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

static bool should_insert(cache_t *cache, const int64_t next_access_vtime) {
  Sieve_Belady_params_t *params = (Sieve_Belady_params_t *)cache->eviction_params;
  double miss_ratio = (double)(params->n_miss) / (double)(cache->n_req);
  int64_t n_req_thresh = (double)(cache->cache_size) / miss_ratio;
  // printf("%ld %ld %lf %ld\n", 
  //        next_access_vtime - cache->n_req, cache->cache_size, miss_ratio, n_req_thresh);
  if (next_access_vtime - cache->n_req <= n_req_thresh) {
    return true;
  } else {
    return false;
  }
}

/**
 * @brief find an object in the cache
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return the object or NULL if not found
 */
static cache_obj_t *Sieve_Belady_find(cache_t *cache, const request_t *req,
                                      const bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);
  if (cache_obj != NULL && update_cache) {
    cache_obj->sieve.freq = 1;
  }

  return cache_obj;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * eviction should be
 * performed before calling this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
static cache_obj_t *Sieve_Belady_insert(cache_t *cache, const request_t *req) {
  Sieve_Belady_params_t *params = cache->eviction_params;

  cache_obj_t *obj = NULL;

#ifdef USE_BELADY_INSERTION
  if (should_insert(cache, req->next_access_vtime)) {
    obj = cache_insert_base(cache, req);
    prepend_obj_to_head(&params->q_head, &params->q_tail, obj);
    obj->sieve.freq = 0;
    // obj->sieve.new_obj = true;
  }
#else
  obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);
  obj->sieve.freq = 0;
  // obj->sieve.new_obj = true;
#endif

  return obj;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 * @param evicted_obj if not NULL, return the evicted object to caller
 */
static void Sieve_Belady_evict(cache_t *cache, const request_t *req) {
  Sieve_Belady_params_t *params = cache->eviction_params;
  cache_obj_t *obj = params->pointer;

  /* if we have run one full around or first eviction */
  if (obj == NULL) {
    obj = params->q_tail;
  }

  /* find the first untouched */
  while (obj != NULL && should_insert(cache, obj->misc.next_access_vtime)) {
    // while (obj != NULL && obj->sieve.freq > 0) {
    obj->sieve.freq -= 1;
    obj = obj->queue.prev;
  }

  /* if we have finished one around, start from the tail */
  if (obj == NULL) {
    obj = params->q_tail;
    while (obj != NULL && should_insert(cache, obj->misc.next_access_vtime)) {
      // while (obj != NULL && obj->sieve.freq > 0) {
      obj->sieve.freq -= 1;
      obj = obj->queue.prev;
    }
  }

  if (obj == NULL) {
    printf("here\n");
    obj = params->pointer;
  }

  params->pointer = obj->queue.prev;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_evict_base(cache, obj, true);
}

static void Sieve_Belady_remove_obj(cache_t *cache,
                                    cache_obj_t *obj_to_remove) {
  DEBUG_ASSERT(obj_to_remove != NULL);
  Sieve_Belady_params_t *params = cache->eviction_params;
  if (obj_to_remove == params->pointer) {
    params->pointer = obj_to_remove->queue.prev;
  }
  remove_obj_from_list(&params->q_head, &params->q_tail, obj_to_remove);
  cache_remove_obj_base(cache, obj_to_remove, true);
}

/**
 * @brief remove an object from the cache
 * this is different from cache_evict because it is used to for user trigger
 * remove, and eviction is used by the cache to make space for new objects
 *
 * it needs to call cache_remove_obj_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param obj_id
 * @return true if the object is removed, false if the object is not in the
 * cache
 */
static bool Sieve_Belady_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  Sieve_Belady_remove_obj(cache, obj);

  return true;
}

static void Sieve_Belady_verify(cache_t *cache) {
  Sieve_Belady_params_t *params = cache->eviction_params;
  int64_t n_obj = 0, n_byte = 0;
  cache_obj_t *obj = params->q_head;

  while (obj != NULL) {
    assert(hashtable_find_obj_id(cache->hashtable, obj->obj_id) != NULL);
    n_obj++;
    n_byte += obj->obj_size;
    obj = obj->queue.next;
  }

  assert(n_obj == cache->get_n_obj(cache));
  assert(n_byte == cache->get_occupied_byte(cache));
}

#ifdef __cplusplus
}
#endif
