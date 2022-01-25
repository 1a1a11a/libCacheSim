

#include "../include/libCacheSim/evictionAlgo/LHD.h"
#include "../dataStructure/hashtable/hashtable.h"
#include <cassert>
#include "lhd.hpp"
#include "repl.hpp"

using namespace repl;

cache_t *LHD_init(common_cache_params_t ccache_params, void *init_params) {
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  if (ccache_params.default_ttl < 30 * 86400) {
    ERROR("LHD does not support expiration\n");
    abort();
  }
#endif

  cache_t *cache = cache_struct_init("LHD", ccache_params);
  cache->cache_init = LHD_init;
  cache->cache_free = LHD_free;
  cache->get = LHD_get;
  cache->check = LHD_check;
  cache->insert = LHD_insert;
  cache->evict = LHD_evict;
  cache->remove = LHD_remove;

  auto *params = my_malloc(LHD_params_t);
  memset(params, 0, sizeof(LHD_params_t));
  cache->eviction_params = params;

  if (init_params != nullptr) {
    auto *LHD_init_params = static_cast<LHD_init_params_t *>(init_params);
    params->admission = LHD_init_params->admission;
    params->associativity = LHD_init_params->associativity;
  }

  if (params->admission == 0) {
    params->admission = 8;
  }
  if(params->associativity == 0) {
    params->associativity = 32;
  }

  params->LHD_cache = static_cast<void *> (new LHD(params->associativity, params->admission, cache));
  return cache;
}

void LHD_free(cache_t *cache) {
  auto *params = static_cast<LHD_params_t *>(cache->eviction_params);
  auto *lhd = static_cast<repl::LHD*>(params->LHD_cache);
  delete lhd;
  cache_struct_free(cache);
}

cache_ck_res_e LHD_check(cache_t *cache, request_t *req, bool update_cache) {
  auto *params = static_cast<LHD_params_t *>(cache->eviction_params);
  auto *lhd = static_cast<repl::LHD*>(params->LHD_cache);

  auto id = repl::candidate_t::make(req);
  auto itr = lhd->sizeMap.find(id);
  bool hit = (itr != lhd->sizeMap.end());

  if (update_cache) {
    cache->n_req += 1;
  }

  if (!hit) {
    return cache_ck_miss;
  }

  if (update_cache) {
    if (itr->second != req->obj_size) {
      cache->occupied_size -= itr->second;
      cache->occupied_size += req->obj_size;
      itr->second = req->obj_size;
    }
    lhd->update(id, req);
  }

  return cache_ck_hit;
}


cache_ck_res_e LHD_get(cache_t *cache, request_t *req) {
  return cache_get_base(cache, req);
}

void LHD_insert(cache_t *cache, request_t *req) {
  auto *params = static_cast<LHD_params_t *>(cache->eviction_params);
  auto *lhd = static_cast<repl::LHD*>(params->LHD_cache);
  auto id = repl::candidate_t::make(req);

  lhd->sizeMap[id] = req->obj_size;
  lhd->update(id, req);

  cache->occupied_size += req->obj_size + cache->per_obj_overhead;
  cache->n_obj += 1;
}

void LHD_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  auto *params = static_cast<LHD_params_t *>(cache->eviction_params);
  auto *lhd = static_cast<repl::LHD*>(params->LHD_cache);

  repl::candidate_t victim = lhd->rank(req);
  auto victimItr = lhd->sizeMap.find(victim);
  if (victimItr == lhd->sizeMap.end()) {
    std::cerr << "Couldn't find victim: " << victim << std::endl;
  }
  assert(victimItr != lhd->sizeMap.end());
//  printf("evict %llu size %u\n", victimItr->first.id, victimItr->second);

  lhd->replaced(victim);
  lhd->sizeMap.erase(victimItr);
  DEBUG_ASSERT(cache->occupied_size >= victimItr->second);
  cache->occupied_size -= (victimItr->second + cache->per_obj_overhead);
  cache->n_obj -= 1;

  if (evicted_obj != nullptr) {
    // return evicted object to caller
    evicted_obj->obj_size = victimItr->second;
    evicted_obj->obj_id = victim.id;
  }
}

void LHD_remove(cache_t *cache, obj_id_t obj_id) {
  auto *params = static_cast<LHD_params_t *>(cache->eviction_params);
  auto *lhd = static_cast<repl::LHD*>(params->LHD_cache);
  repl::candidate_t id{DEFAULT_APP_ID, (int64_t) obj_id};

  auto itr = lhd->sizeMap.find(id);
  if (itr == lhd->sizeMap.end()) {
    WARN("obj to remove is not in the cache\n");
    return;
  }

  cache->occupied_size -= (itr->second + cache->per_obj_overhead);
  cache->n_obj -= 1;
  lhd->sizeMap.erase(itr);
  auto idx = lhd->indices[id];
  lhd->indices.erase(id);
  lhd->tags[idx] = lhd->tags.back();
  lhd->tags.pop_back();

  if (idx < lhd->tags.size()) {
    lhd->indices[lhd->tags[idx].id] = idx;
  }
}
