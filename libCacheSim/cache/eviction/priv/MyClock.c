

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/cache.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  cache_obj_t *pointer;

  int64_t n_miss;
  int64_t n_new_obj;  // used to track the number of objects since last pointer
                      // went back to the tail (the oldest)
} MyClock_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void MyClock_free(cache_t *cache);
static bool MyClock_get(cache_t *cache, const request_t *req);
static cache_obj_t *MyClock_find(cache_t *cache, const request_t *req,
                                 const bool update_cache);
static cache_obj_t *MyClock_insert(cache_t *cache, const request_t *req);
static cache_obj_t *MyClock_to_evict(cache_t *cache, const request_t *req);
static void MyClock_evict(cache_t *cache, const request_t *req);
static bool MyClock_remove(cache_t *cache, const obj_id_t obj_id);

static void MyClock_verify(cache_t *cache);

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
cache_t *MyClock_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("MyClock", ccache_params, cache_specific_params);
  cache->cache_init = MyClock_init;
  cache->cache_free = MyClock_free;
  cache->get = MyClock_get;
  cache->find = MyClock_find;
  cache->insert = MyClock_insert;
  cache->evict = MyClock_evict;
  cache->remove = MyClock_remove;
  cache->to_evict = MyClock_to_evict;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 1;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc(MyClock_params_t);
  memset(cache->eviction_params, 0, sizeof(MyClock_params_t));
  MyClock_params_t *params = (MyClock_params_t *)cache->eviction_params;
  params->pointer = NULL;
  params->q_head = NULL;
  params->q_tail = NULL;
  params->n_miss = 0;

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void MyClock_free(cache_t *cache) {
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

static bool MyClock_get(cache_t *cache, const request_t *req) {
  MyClock_params_t *params = (MyClock_params_t *)cache->eviction_params;

  bool ck_hit = cache_get_base(cache, req);
  if (!ck_hit) {
    params->n_miss++;
  }

  // if (cache->n_req % 1000 == 0) {
  //   printf("sieve %ld %ld %lf\n", cache->cache_size, cache->n_req,
  //          (double)params->n_new_obj / (double)cache->cache_size);
  // }

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
static cache_obj_t *MyClock_find(cache_t *cache, const request_t *req,
                                 const bool update_cache) {
  cache_obj_t *cache_obj = cache_find_base(cache, req, update_cache);
  if (cache_obj != NULL && update_cache) {
    cache_obj->myclock.freq = 1;
    // cache_obj->myclock.freq += 1;
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
static cache_obj_t *MyClock_insert(cache_t *cache, const request_t *req) {
  MyClock_params_t *params = cache->eviction_params;
  cache_obj_t *obj = cache_insert_base(cache, req);
  prepend_obj_to_head(&params->q_head, &params->q_tail, obj);
  obj->myclock.freq = 0;
  obj->myclock.new_obj = true;
  params->n_new_obj++;

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
static cache_obj_t *MyClock_to_evict_with_freq(cache_t *cache,
                                               const request_t *req,
                                               int to_evict_freq) {
  MyClock_params_t *params = cache->eviction_params;
  cache_obj_t *pointer = params->pointer;
  cache_obj_t *old_pointer = pointer;

  /* if we have run one full around or first eviction */
  if (pointer == NULL) pointer = params->q_tail;

  /* find the first untouched */
  while (pointer != NULL && pointer->myclock.freq > to_evict_freq) {
    pointer = pointer->queue.prev;
  }

  /* if we have finished one around, start from the tail */
  if (pointer == NULL) {
    pointer = params->q_tail;
    while (pointer != NULL && pointer->myclock.freq > to_evict_freq) {
      pointer = pointer->queue.prev;
    }
  }

  if (pointer == NULL) return NULL;

  return pointer;
}

static cache_obj_t *MyClock_to_evict(cache_t *cache, const request_t *req) {
  // because we do not change the frequency of the object,
  // if all objects have frequency 1, we may return NULL
  int to_evict_freq = 0;

  cache_obj_t *obj_to_evict =
      MyClock_to_evict_with_freq(cache, req, to_evict_freq);

  while (obj_to_evict == NULL) {
    to_evict_freq += 1;

    obj_to_evict = MyClock_to_evict_with_freq(cache, req, to_evict_freq);
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
static void MyClock_evict(cache_t *cache, const request_t *req) {
  MyClock_params_t *params = cache->eviction_params;
  cache_obj_t *obj = params->pointer;

  /* if we have run one full around or first eviction */
  if (obj == NULL) {
    obj = params->q_tail;
  }

  /* find the first untouched */
  while ((obj != NULL && obj->myclock.freq > 0)) {
    obj->myclock.freq -= 1;
    if (obj->myclock.new_obj) params->n_new_obj -= 1;

    obj->myclock.new_obj = false;
    obj = obj->queue.prev;
  }

  /* if we have finished one around, start from the tail */
  if (obj == NULL) {
    obj = params->q_tail;
    while (obj != NULL && obj->myclock.freq > 0) {
      obj->myclock.freq -= 1;
      obj->myclock.new_obj = false;
      obj = obj->queue.prev;
    }
  }

  if (obj->myclock.new_obj) params->n_new_obj -= 1;
  params->pointer = obj->queue.prev;
  remove_obj_from_list(&params->q_head, &params->q_tail, obj);
  cache_evict_base(cache, obj, true);
}

static void MyClock_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  DEBUG_ASSERT(obj_to_remove != NULL);
  MyClock_params_t *params = cache->eviction_params;
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
static bool MyClock_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  MyClock_remove_obj(cache, obj);

  return true;
}

static void MyClock_verify(cache_t *cache) {
  MyClock_params_t *params = cache->eviction_params;
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
