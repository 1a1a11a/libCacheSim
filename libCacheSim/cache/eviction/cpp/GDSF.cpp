/* GDSF: greedy dual frequency size */

#include "../../../include/libCacheSim/evictionAlgo/GDSF.h"

#include <cassert>

#include "abstractRank.hpp"
#include "hashtable.h"

namespace eviction {
class GDSF : public abstractRank {
 public:
  GDSF() = default;

  double pri_last_evict = 0.0;
};
}  // namespace eviction

cache_t *GDSF_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("GDSF", ccache_params);
  cache->eviction_params = reinterpret_cast<void *>(new eviction::GDSF);

  cache->cache_init = GDSF_init;
  cache->cache_free = GDSF_free;
  cache->get = cache_get_base;
  cache->check = GDSF_check;
  cache->insert = GDSF_insert;
  cache->evict = GDSF_evict;
  cache->remove = GDSF_remove;

  if (ccache_params.consider_obj_metadata) {
    // freq + priority
    cache->per_obj_metadata_size = 8;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  return cache;
}

void GDSF_free(cache_t *cache) {
  delete reinterpret_cast<eviction::GDSF *>(cache->eviction_params);
  cache_struct_free(cache);
}

cache_ck_res_e GDSF_check(cache_t *cache, const request_t *req,
                          const bool update_cache) {
  auto *gdsf = reinterpret_cast<eviction::GDSF *>(cache->eviction_params);
  cache_obj_t *obj;
  auto res = cache_check_base(cache, req, update_cache, &obj);
  /* this does not consider object size change */
  if (obj != nullptr && update_cache) {
    /* update frequency */
    obj->lfu.freq += 1;

    auto itr = gdsf->itr_map[obj];
    gdsf->pq.erase(itr);

    double pri = gdsf->pri_last_evict + (double)(obj->lfu.freq) * 1.0e6 / obj->obj_size;
    itr = gdsf->pq.emplace(obj, pri, cache->n_req).first;
    gdsf->itr_map[obj] = itr;
  }

  return res;
}

cache_obj_t *GDSF_insert(cache_t *cache, const request_t *req) {
  auto *gdsf = reinterpret_cast<eviction::GDSF *>(cache->eviction_params);

  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->lfu.freq = 1;

  double pri = gdsf->pri_last_evict + 1.0e6 / obj->obj_size;

  auto itr = gdsf->pq.emplace(obj, pri, cache->n_req).first;
  gdsf->itr_map[obj] = itr;

  return obj;
}

void GDSF_evict(cache_t *cache, const request_t *req,
                cache_obj_t *evicted_obj) {
  auto *gdsf = reinterpret_cast<eviction::GDSF *>(cache->eviction_params);
  eviction::pq_node_type p = gdsf->pick_lowest_score();
  cache_obj_t *obj = p.obj;
  if (evicted_obj != nullptr) {
    memcpy(evicted_obj, obj, sizeof(cache_obj_t));
  }

  gdsf->pri_last_evict = p.priority;
  cache_remove_obj_base(cache, obj);
}

void GDSF_remove_obj(cache_t *cache, cache_obj_t *obj) {
  auto *gdsf = reinterpret_cast<eviction::GDSF *>(cache->eviction_params);
  gdsf->remove_obj(cache, obj);
}

void GDSF_remove(cache_t *cache, const obj_id_t obj_id) {
  auto *gdsf = reinterpret_cast<eviction::GDSF *>(cache->eviction_params);
  gdsf->remove(cache, obj_id);
}
