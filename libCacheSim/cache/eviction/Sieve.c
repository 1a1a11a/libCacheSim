

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/cache.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  cache_obj_t *pointer;
} Sieve_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
static void Sieve_free(cache_t *cache);
static bool Sieve_get(cache_t *cache, const request_t *req);
static cache_obj_t *Sieve_find(cache_t *cache, const request_t *req,
                               const bool update_cache);
static cache_obj_t *Sieve_insert(cache_t *cache, const request_t *req);
static cache_obj_t *Sieve_to_evict(cache_t *cache, const request_t *req);
static void Sieve_evict(cache_t *cache, const request_t *req);
static bool Sieve_remove(cache_t *cache, const obj_id_t obj_id);

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
cache_t *Sieve_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("Sieve", ccache_params, cache_specific_params);
  cache->cache_init = Sieve_init;
  cache->cache_free = Sieve_free;
  cache->get = Sieve_get;
  cache->find = Sieve_find;
  cache->insert = Sieve_insert;
  cache->evict = Sieve_evict;
  cache->remove = Sieve_remove;
  cache->to_evict = Sieve_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 1;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc(Sieve_params_t);
  memset(cache->eviction_params, 0, sizeof(Sieve_params_t));
  Sieve_params_t *params = (Sieve_params_t *)cache->eviction_params;
  params->pointer = NULL;
  params->q_head = NULL;
  params->q_tail = NULL;

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void Sieve_free(cache_t *cache) {
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

static bool Sieve_get(cache_t *cache, const request_t *req) {
  bool ck_hit = cache_get_base(cache, req);
  return ck_hit;
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************

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
static cache_obj_t *Sieve_find(cache_t *cache, const request_t *req,
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
static cache_obj_t *Sieve_insert(cache_t *cache, const request_t *req) {
  Sieve_params_t *params = cache->eviction_params;
  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);
  obj->sieve.freq = 0;

  return obj;
}

/**
 * @brief find the object to be evicted
 * this function does not actually evict the object or update metadata
 * not all eviction algorithms support this function
 * because the eviction logic cannot be decoupled from finding eviction
 * candidate, so use assert(false) if you cannot support this function
 *
 * @param cache the cache
 * @return the object to be evicted
 */
static cache_obj_t *Sieve_to_evict_with_freq(cache_t *cache,
                                             const request_t *req,
                                             int to_evict_freq) {
  Sieve_params_t *params = cache->eviction_params;
  cache_obj_t *pointer = params->pointer;

  /* if we have run one full around or first eviction */
  if (pointer == NULL) pointer = params->q_tail;

  /* find the first untouched */
  while (pointer != NULL && pointer->sieve.freq > to_evict_freq) {
    pointer = pointer->queue.prev;
  }

  /* if we have finished one around, start from the tail */
  if (pointer == NULL) {
    pointer = params->q_tail;
    while (pointer != NULL && pointer->sieve.freq > to_evict_freq) {
      pointer = pointer->queue.prev;
    }
  }

  if (pointer == NULL) return NULL;

  return pointer;
}

static cache_obj_t *Sieve_to_evict(cache_t *cache, const request_t *req) {
  // because we do not change the frequency of the object,
  // if all objects have frequency 1, we may return NULL
  int to_evict_freq = 0;

  cache_obj_t *obj_to_evict =
      Sieve_to_evict_with_freq(cache, req, to_evict_freq);

  while (obj_to_evict == NULL) {
    to_evict_freq += 1;

    obj_to_evict = Sieve_to_evict_with_freq(cache, req, to_evict_freq);
  }

  return obj_to_evict;
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
static void Sieve_evict(cache_t *cache, const request_t *req) {
  Sieve_params_t *params = cache->eviction_params;

  /* if we have run one full around or first eviction */
  cache_obj_t *obj = params->pointer == NULL ? params->q_tail : params->pointer;

  while (obj->sieve.freq > 0) {
    obj->sieve.freq -= 1;
    obj = obj->queue.prev == NULL ? params->q_tail : obj->queue.prev;
  }

  params->pointer = obj->queue.prev;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_evict_base(cache, obj, true);
}

static void Sieve_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  DEBUG_ASSERT(obj_to_remove != NULL);
  Sieve_params_t *params = cache->eviction_params;
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
static bool Sieve_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  Sieve_remove_obj(cache, obj);

  return true;
}

static void Sieve_verify(cache_t *cache) {
  Sieve_params_t *params = cache->eviction_params;
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
