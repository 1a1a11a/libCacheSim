
/**
 * a LFU module written in C++
 * note that the miss ratio of LFU cpp can be different from LFUFast
 * this is because of the difference in handling objects of same frequency
 *
 */

#include <cassert>

#include "../../../include/libCacheSim/evictionAlgo/LFUCpp.h"
#include "abstractRank.hpp"
#include "hashtable.h"

namespace eviction {
class LFUCpp : public abstractRank {
 public:
  LFUCpp() = default;
  ;
};
}  // namespace eviction

cache_t *LFUCpp_init(common_cache_params_t ccache_params,
                     const char *init_params) {
  cache_t *cache = cache_struct_init("LFUCpp", ccache_params);
  auto *lfu = new eviction::LFUCpp();
  cache->eviction_params = lfu;

  cache->cache_init = LFUCpp_init;
  cache->cache_free = LFUCpp_free;
  cache->get = LFUCpp_get;
  cache->check = LFUCpp_check;
  cache->insert = LFUCpp_insert;
  cache->evict = LFUCpp_evict;
  cache->remove = LFUCpp_remove;

  if (ccache_params.consider_obj_metadata) {
    // freq
    cache->per_obj_metadata_size = 8;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  return cache;
}

void LFUCpp_free(cache_t *cache) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);
  delete lfu;
  cache_struct_free(cache);
}

cache_ck_res_e LFUCpp_check(cache_t *cache, const request_t *req,
                            const bool update_cache) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);
  cache_obj_t *obj;
  auto res = cache_check_base(cache, req, update_cache, &obj);
  if (obj != nullptr && update_cache) {
    obj->lfu.freq++;
    auto itr = lfu->itr_map[obj];
    lfu->pq.erase(itr);
    itr = lfu->pq.emplace(obj, (double)obj->lfu.freq, cache->n_req).first;
    lfu->itr_map[obj] = itr;
  }

  return res;
}

cache_obj_t *LFUCpp_insert(cache_t *cache, const request_t *req) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);

  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->lfu.freq = 1;

  auto itr = lfu->pq.emplace_hint(lfu->pq.begin(), obj, 1.0, cache->n_req);
  lfu->itr_map[obj] = itr;
  DEBUG_ASSERT(lfu->itr_map.size() == cache->n_obj);

  return obj;
}

void LFUCpp_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *evicted_obj) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);
  eviction::pq_node_type p = lfu->pick_lowest_score();
  cache_obj_t *obj = p.obj;
  if (evicted_obj != nullptr) memcpy(evicted_obj, obj, sizeof(cache_obj_t));

  cache_remove_obj_base(cache, obj);
}

cache_ck_res_e LFUCpp_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

void LFUCpp_remove_obj(cache_t *cache, cache_obj_t *obj) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);
  lfu->remove_obj(cache, obj);
}

void LFUCpp_remove(cache_t *cache, const obj_id_t obj_id) {
  auto *lfu = static_cast<eviction::LFUCpp *>(cache->eviction_params);
  lfu->remove(cache, obj_id);
}





