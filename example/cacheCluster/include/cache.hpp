
//
//  cache.cpp
//  CDNSimulator
//
//  Created by Juncheng on 11/18/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifndef CDNSIMULATOR_CACHE_HPP
#define CDNSIMULATOR_CACHE_HPP

#include <algorithm>
#include <iostream>
#include <inttypes.h>

#include "libCacheSim/cache.h"
#include "libCacheSim/evictionAlgo.h"

namespace CDNSimulator {

class Cache {
 private:
  cache_t* _cache = nullptr;
  unsigned long _cache_size;

 public:
  Cache(const Cache& other) = delete;

  Cache(Cache&& other) {
    this->_cache = clone_cache(other._cache);
    this->_cache_size = other._cache_size;
  };

  Cache& operator=(const Cache& other) = delete;

  Cache& operator=(Cache&& other) = delete;

  Cache(const uint64_t cache_size, const std::string cache_alg,
        const uint32_t hashpower = 20) {
    this->_cache_size = cache_size;

    common_cache_params_t params;
    params.cache_size = cache_size;
    params.consider_obj_metadata = false;
    params.hashpower = hashpower;

    if (strcasecmp(cache_alg.c_str(), "lru") == 0) {
      this->_cache = LRU_init(params, NULL);
    } else if (strcasecmp(cache_alg.c_str(), "fifo") == 0) {
      this->_cache = FIFO_init(params, NULL);
    } else {
      ERROR("unknown cache replacement algorithm %s\n", cache_alg.c_str());
      abort();
    }

    srand((unsigned int)time(nullptr));
  }

  inline bool get(request_t* req) {
    return this->_cache->get(this->_cache, req);
  }

  ~Cache() { this->_cache->cache_free(this->_cache); }
};
}  // namespace CDNSimulator

#endif
