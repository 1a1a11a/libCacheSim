
/**
 * a LFU module written in C++
 * note that the miss ratio of LFU cpp can be different from LFUFast
 * this is because of the difference in handling objects of same frequency
 *
 */

#include <cassert>

#include "abstractRank.hpp"

namespace eviction {
class LFUCpp : public abstractRank {
 public:
  LFUCpp() = default;
  ;
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

static void LFUCpp_free(cache_t *cache);
static bool LFUCpp_get(cache_t *cache, const request_t *req);
static cache_obj_t *LFUCpp_find(cache_t *cache, const request_t *req,
                                const bool update_cache);
static cache_obj_t *LFUCpp_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LFUCpp_to_evict(cache_t *cache, const request_t *req);
static void LFUCpp_evict(cache_t *cache, const request_t *req);
static bool LFUCpp_remove(cache_t *cache, const obj_id_t obj_id);

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
cache_t *LFUCpp_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("LFUCpp", ccache_params, cache_specific_params);
  auto *lfu = new eviction::LFUCpp();
  cache->eviction_params = lfu;

  cache->cache_init = LFUCpp_init;
  cache->cache_free = LFUCpp_free;
  cache->get = LFUCpp_get;
  cache->find = LFUCpp_find;
  cache->insert = LFUCpp_insert;
  cache->evict = LFUCpp_evict;
  cache->remove = LFUCpp_remove;

  if (ccache_params.consider_obj_metadata) {
    // freq
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
static void LFUCpp_free(cache_t *cache) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);
  delete lfu;
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
static bool LFUCpp_get(cache_t *cache, const request_t *req) {
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
static cache_obj_t *LFUCpp_find(cache_t *cache, const request_t *req,
                                const bool update_cache) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);
  if (obj != nullptr && update_cache) {
    obj->lfu.freq++;
    auto itr = lfu->pq_map[obj];
    lfu->pq.erase(itr);
    eviction::pq_node_type new_node = {obj, (double)obj->lfu.freq,
                                       cache->n_req};
    lfu->pq.insert(new_node);
    lfu->pq_map[obj] = new_node;
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
static cache_obj_t *LFUCpp_insert(cache_t *cache, const request_t *req) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);

  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->lfu.freq = 1;

  eviction::pq_node_type new_node = {obj, 1.0, cache->n_req};
  lfu->pq.insert(new_node);
  lfu->pq_map[obj] = new_node;
  DEBUG_ASSERT((int64_t)lfu->pq_map.size() == cache->n_obj);

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
static cache_obj_t *LFUCpp_to_evict(cache_t *cache, const request_t *req) {
  // does not support to_evict
  DEBUG_ASSERT(false);
  return NULL;
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
static void LFUCpp_evict(cache_t *cache, const request_t *req) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);
  eviction::pq_node_type p = lfu->pop_lowest_score();
  cache_obj_t *obj = p.obj;

  cache_remove_obj_base(cache, obj, true);
}

static void LFUCpp_remove_obj(cache_t *cache, cache_obj_t *obj) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);
  lfu->remove_obj(cache, obj);
  cache_remove_obj_base(cache, obj, true);
}

static bool LFUCpp_remove(cache_t *cache, const obj_id_t obj_id) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == nullptr) {
    return false;
  }
  bool ret = lfu->remove(cache, obj_id);
  cache_remove_obj_base(cache, obj, true);
  return true;
}

#ifdef __cplusplus
}
#endif
