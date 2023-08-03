//
//  cacheCluster.cpp
//  CDNSimulator
//
//  Created by Juncheng Yang on 7/13/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//

#include "include/cacheCluster.hpp"

#include "include/consistentHash.h"
// #include <iomanip>

namespace CDNSimulator {

bool CacheCluster::get(request_t *req) {
  // find the server idx
  uint64_t idx =
      ch_ring_get_server_from_uint64(req->obj_id, this->_ring);

  // find the server
  CacheServer &server = this->_cache_servers_vec.at(idx);

  bool hit = server.get(req);

  return hit;
}

CacheCluster::~CacheCluster() {
  if (_ring != NULL) {
    ch_ring_destroy_ring(_ring);
  }
}
}  // namespace CDNSimulator
