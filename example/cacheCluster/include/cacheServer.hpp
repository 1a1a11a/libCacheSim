//
//  cacheServer.cpp
//  CDNSimulator
//
//  Created by Juncheng on 7/11/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//

#ifndef CACHE_SERVER_HPP
#define CACHE_SERVER_HPP

#include <inttypes.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "cache.hpp"
#include "libCacheSim/cache.h"
#include "libCacheSim/evictionAlgo.h"
#include "libCacheSim/logging.h"

namespace CDNSimulator {

class CacheServer {
 public:
  unsigned long _server_id;
  // the number of caches in this server

  // the caches in this server
  std::vector<Cache> _cache_vec;

  /* availability simulation */
  bool _available = true;

  CacheServer(unsigned long server_id) { this->_server_id = server_id; }

  CacheServer(const CacheServer& other) = delete;

  CacheServer(CacheServer&& other) {
    this->_server_id = other._server_id;
    this->_cache_vec = std::move(other._cache_vec);
    this->_available = other._available;
  };

  CacheServer& operator=(const CacheServer& other) = delete;

  CacheServer& operator=(CacheServer&& other) {
    this->_server_id = other._server_id;
    this->_cache_vec = std::move(other._cache_vec);
    this->_available = other._available;
    return *this;
  };

  /**
   * @brief add a cache to this server
   *      return the index of the cache in this server
   *
   * @param cache
   * @return size_t
   */
  inline size_t add_cache(Cache&& cache) {
    // this->_cache_vec.emplace_back(cache_size, cache_alg, hashpower);

    this->_cache_vec.push_back(std::move(cache));
    return this->_cache_vec.size() - 1;
  }

  inline bool get(request_t* req) {
    if (!_available) return false;

    for (int i = 0; i < this->_cache_vec.size(); i++) {
      // check each cache in this server
      bool hit = this->_cache_vec.at(i).get(req);

      if (hit) return true;
    }

    return false;
  }

  inline bool set_unavailable() {
    _available = false;
    return _available;
  }

  inline bool set_available() {
    _available = true;
    return _available;
  }

  inline bool is_available() { return _available; }

  inline unsigned long get_server_id() { return this->_server_id; }

  ~CacheServer() = default;
};

}  // namespace CDNSimulator

#endif /* CACHE_SERVER_HPP */
