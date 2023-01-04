

#include <assert.h>

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo/LHD.h"
#include "lhd.hpp"
#include "repl.hpp"

using namespace repl;

typedef struct {
  void *LHD_cache;

  int associativity;
  int admission;

} LHD_params_t;

cache_t *LHD_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params) {
#ifdef SUPPORT_TTL 
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
  cache->init_params = cache_specific_params;
  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  }

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 3 + 1;  // two age, one time stamp
  } else {
    cache->obj_md_size = 0;
  }

  auto *params = my_malloc(LHD_params_t);
  memset(params, 0, sizeof(LHD_params_t));
  cache->eviction_params = params;

  if (params->admission == 0) {
    params->admission = 8;
  }
  if (params->associativity == 0) {
    params->associativity = 32;
  }

  params->LHD_cache = static_cast<void *>(
      new LHD(params->associativity, params->admission, cache));

  return cache;
}

void LHD_free(cache_t *cache) {
  auto *params = static_cast<LHD_params_t *>(cache->eviction_params);
  auto *lhd = static_cast<repl::LHD *>(params->LHD_cache);
  delete lhd;
  my_free(sizeof(LHD_params_t), params);
  cache_struct_free(cache);
}

bool LHD_check(cache_t *cache, const request_t *req, const bool update_cache) {
  auto *params = static_cast<LHD_params_t *>(cache->eviction_params);
  auto *lhd = static_cast<repl::LHD *>(params->LHD_cache);

  auto id = repl::candidate_t::make(req);
  auto itr = lhd->sizeMap.find(id);
  bool hit = (itr != lhd->sizeMap.end());

  if (!hit) {
    return hit;
  }

  if (update_cache) {
    if (itr->second != req->obj_size) {
      cache->occupied_size -= itr->second;
      cache->occupied_size += req->obj_size;
      itr->second = req->obj_size;
    }
    lhd->update(id, req);
  }

  return hit;
}

bool LHD_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_obj_t *LHD_insert(cache_t *cache, const request_t *req) {
  auto *params = static_cast<LHD_params_t *>(cache->eviction_params);
  auto *lhd = static_cast<repl::LHD *>(params->LHD_cache);
  auto id = repl::candidate_t::make(req);

#if defined(TRACK_EVICTION_R_AGE) || defined(TRACK_EVICTION_V_AGE)
  id.create_time = CURR_TIME(cache, req);
#endif

  lhd->sizeMap[id] = req->obj_size;
  lhd->update(id, req);

  cache->occupied_size += req->obj_size + cache->obj_md_size;
  cache->n_obj += 1;

  return NULL;
}

void LHD_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj) {
  auto *params = static_cast<LHD_params_t *>(cache->eviction_params);
  auto *lhd = static_cast<repl::LHD *>(params->LHD_cache);

  repl::candidate_t victim = lhd->rank(req);
  auto victimItr = lhd->sizeMap.find(victim);
  if (victimItr == lhd->sizeMap.end()) {
    std::cerr << "Couldn't find victim: " << victim << std::endl;
  }
  assert(victimItr != lhd->sizeMap.end());

  DEBUG_ASSERT(cache->occupied_size >= victimItr->second);
  cache->occupied_size -= (victimItr->second + cache->obj_md_size);
  cache->n_obj -= 1;

#if defined(TRACK_EVICTION_R_AGE) || defined(TRACK_EVICTION_V_AGE)
  record_eviction_age(cache, CURR_TIME(cache, req) - victim.create_time);
#endif

  if (evicted_obj != nullptr) {
    // return evicted object to caller
    evicted_obj->obj_size = victimItr->second;
    evicted_obj->obj_id = victim.id;
  }

  lhd->replaced(victim);
  lhd->sizeMap.erase(victimItr);
}

bool LHD_remove(cache_t *cache, const obj_id_t obj_id) {
  auto *params = static_cast<LHD_params_t *>(cache->eviction_params);
  auto *lhd = static_cast<repl::LHD *>(params->LHD_cache);
  repl::candidate_t id{DEFAULT_APP_ID, (int64_t)obj_id};

  auto itr = lhd->sizeMap.find(id);
  if (itr == lhd->sizeMap.end()) {
    return false;
  }

  cache->occupied_size -= (itr->second + cache->obj_md_size);
  cache->n_obj -= 1;
  lhd->sizeMap.erase(itr);
  auto idx = lhd->indices[id];
  lhd->indices.erase(id);
  lhd->tags[idx] = lhd->tags.back();
  lhd->tags.pop_back();

  if (idx < lhd->tags.size()) {
    lhd->indices[lhd->tags[idx].id] = idx;
  }

  return true;
}
