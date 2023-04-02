/* GDSF: greedy dual frequency size */

#include <cassert>

#include "abstractRank.hpp"

namespace eviction {
class GDSF : public abstractRank {
 public:
  GDSF() = default;

  double pri_last_evict = 0.0;
};
}  // namespace eviction

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

cache_t *GDSF_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);
static void GDSF_free(cache_t *cache);
static bool GDSF_get(cache_t *cache, const request_t *req);

static cache_obj_t *GDSF_find(cache_t *cache, const request_t *req,
                              const bool update_cache);
static cache_obj_t *GDSF_insert(cache_t *cache, const request_t *req);
static cache_obj_t *GDSF_to_evict(cache_t *cache, const request_t *req);
static void GDSF_evict(cache_t *cache, const request_t *req);
static bool GDSF_remove(cache_t *cache, const obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief initialize the cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, see parse_params
 * function or use -e "print" with the cachesim binary
 */
cache_t *GDSF_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("GDSF", ccache_params, cache_specific_params);
  cache->eviction_params = reinterpret_cast<void *>(new eviction::GDSF);

  cache->cache_init = GDSF_init;
  cache->cache_free = GDSF_free;
  cache->get = GDSF_get;
  cache->find = GDSF_find;
  cache->insert = GDSF_insert;
  cache->evict = GDSF_evict;
  cache->to_evict = NULL;
  cache->remove = GDSF_remove;

  if (ccache_params.consider_obj_metadata) {
    // freq + priority
    cache->obj_md_size = 8;
  } else {
    cache->obj_md_size = 0;
  }

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void GDSF_free(cache_t *cache) {
  delete reinterpret_cast<eviction::GDSF *>(cache->eviction_params);
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
static bool GDSF_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
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
static cache_obj_t *GDSF_find(cache_t *cache, const request_t *req,
                              const bool update_cache) {
  auto *gdsf = reinterpret_cast<eviction::GDSF *>(cache->eviction_params);
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);
  /* this does not consider object size change */
  if (obj != nullptr && update_cache) {
    /* update frequency */
    obj->lfu.freq += 1;

    auto itr = gdsf->itr_map[obj];
    gdsf->pq.erase(itr);

    double pri =
        gdsf->pri_last_evict + (double)(obj->lfu.freq) * 1.0e6 / obj->obj_size;
    itr = gdsf->pq.emplace(obj, pri, cache->n_req).first;
    gdsf->itr_map[obj] = itr;
  }

  return obj;
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
static cache_obj_t *GDSF_insert(cache_t *cache, const request_t *req) {
  auto *gdsf = reinterpret_cast<eviction::GDSF *>(cache->eviction_params);

  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->lfu.freq = 1;

  double pri = gdsf->pri_last_evict + 1.0e6 / obj->obj_size;

  auto itr = gdsf->pq.emplace(obj, pri, cache->n_req).first;
  gdsf->itr_map[obj] = itr;

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
static cache_obj_t *GDSF_to_evict(cache_t *cache, const request_t *req) {
  auto *gdsf = reinterpret_cast<eviction::GDSF *>(cache->eviction_params);
  eviction::pq_node_type p = gdsf->peek_lowest_score();
  
  return p.obj;
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
static void GDSF_evict(cache_t *cache, const request_t *req) {
  auto *gdsf = reinterpret_cast<eviction::GDSF *>(cache->eviction_params);
  eviction::pq_node_type p = gdsf->pop_lowest_score();
  cache_obj_t *obj = p.obj;

  gdsf->pri_last_evict = p.priority;
  cache_remove_obj_base(cache, obj, true);
}

static void GDSF_remove_obj(cache_t *cache, cache_obj_t *obj) {
  auto *gdsf = reinterpret_cast<eviction::GDSF *>(cache->eviction_params);
  gdsf->remove_obj(cache, obj);
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
static bool GDSF_remove(cache_t *cache, const obj_id_t obj_id) {
  auto *gdsf = reinterpret_cast<eviction::GDSF *>(cache->eviction_params);
  return gdsf->remove(cache, obj_id);
}

#ifdef __cplusplus
}
#endif
