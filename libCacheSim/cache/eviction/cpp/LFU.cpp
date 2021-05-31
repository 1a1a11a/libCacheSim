
/**
 * a LFU module written in C++
 * note that the miss ratio of LFU cpp can be different from LFUFast
 * this is because of the difference in handling objects of same frequency
 *
 */

#include "libCacheSim/evictionAlgo/LFU.h"
#include "hashtable.h"
#include <cassert>

#include "abstractRank.h"


namespace eviction {
class LFU : public abstractRank {
public:
  LFU() = default;;
};
}



cache_t *LFU_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("LFU", ccache_params);
  cache->eviction_params = reinterpret_cast<void *>(new eviction::LFU());

  cache->cache_init = LFU_init;
  cache->cache_free = LFU_free;
  cache->get = LFU_get;
  cache->check = LFU_check;
  cache->insert = LFU_insert;
  cache->evict = LFU_evict;
  cache->remove = LFU_remove;

  return cache;
}

void LFU_free(cache_t *cache) {
  delete reinterpret_cast<eviction::LFU *>(cache->eviction_params);
  cache_struct_free(cache);
}

cache_ck_res_e LFU_check(cache_t *cache, request_t *req, bool update_cache) {
  auto *lfu = reinterpret_cast<eviction::LFU *>(cache->eviction_params);
  cache_obj_t *obj;
  auto res = cache_check_base(cache, req, update_cache, &obj);
  if (obj != nullptr && update_cache)
    lfu->ordered_map[obj] = lfu->ordered_map[obj] + 1;

  return res;
}

cache_ck_res_e LFU_get(cache_t *cache, request_t *req) {
  return cache_get_base(cache, req);
}

void LFU_insert(cache_t *cache, request_t *req) {
  auto *lfu = reinterpret_cast<eviction::LFU *>(cache->eviction_params);
  cache_obj_t *cache_obj = cache_insert_base(cache, req);
  lfu->ordered_map[cache_obj] = 1;
}

void LFU_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  auto *lfu = reinterpret_cast<eviction::LFU *>(cache->eviction_params);
  cache_obj_t *obj = lfu->pick_lowest_score();
  if (evicted_obj != nullptr)
    memcpy(evicted_obj, obj, sizeof(cache_obj_t));

  lfu->ordered_map.erase(obj);
  cache_remove_obj_base(cache, obj);
}

void LFU_remove_obj(cache_t *cache, cache_obj_t *obj) {
  auto *lfu = reinterpret_cast<eviction::LFU *>(cache->eviction_params);
  lfu->ordered_map.erase(obj);
  cache_remove_obj_base(cache, obj);
}


void LFU_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == nullptr) {
    WARNING("obj is not in the cache\n");
    return;
  }
  LFU_remove_obj(cache, obj);
}


