//
//  cacheCluster.hpp
//  CDNSimulator
//
//  Created by Juncheng Yang on 11/18/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifndef CACHE_CLUSTER_HPP
#define CACHE_CLUSTER_HPP

#include <algorithm>
#include <numeric>
#include <vector>

#include "cacheServer.hpp"
#include "consistentHash.h"
// #include "hasher.hpp"
#include "libCacheSim/cache.h"
#include "libCacheSim/logging.h"
#include "libCacheSim/reader.h"

namespace CDNSimulator {

class CacheCluster {
 private:

  // the consistent hash ring
  ring_t *_ring = nullptr;

  // the capacity of each server, used to assign requests to servers
  // server with larger weight will be assigned more requests
  std::vector<double> _server_weights_vec;

  // the cache servers in this cluster
  std::vector<CacheServer> _cache_servers_vec;

 public:
  unsigned long cluster_id;

  // void get_all_cluster_stored_obj();
  // void get_all_server_stored_obj();

  CacheCluster(const CacheCluster &other) = delete;

  CacheCluster(CacheCluster &&other) = default;

  CacheCluster(unsigned long cluster_id = 0) {
    this->cluster_id = cluster_id;
    this->_cache_servers_vec.reserve(128);
    // this->_hasher = myHasher(n_server);
  }

  /**
   * @brief get a request from the cluster, if the requested object is not
   * cached, the object will be loaded from the origin server
   *
   */
  bool get(request_t *req);

  /**
   * @brief add a cache server to the cluster, return the index of the server
   *
   * @param cache_server
   * @return uint64_t
   */
  inline uint64_t add_server(CacheServer &&cache_server,
                             double server_weight = 1.00) {
    this->_cache_servers_vec.push_back(std::move(cache_server));
    this->_server_weights_vec.push_back(server_weight);

    // normalize the server weights
    std::vector<double> server_normalized_weights_vec;
    double sum = std::accumulate(this->_server_weights_vec.begin(),
                                 this->_server_weights_vec.end(), 0.0);

    for (auto weight : this->_server_weights_vec) {
      server_normalized_weights_vec.push_back(weight / sum);
    }

    if (this->_ring != nullptr) {
      ch_ring_destroy_ring(this->_ring);
    }

    this->_ring = ch_ring_create_ring(this->_cache_servers_vec.size(),
                                      server_normalized_weights_vec.data());

    return this->_cache_servers_vec.size() - 1;
  }

  /**
   * @brief remove a cache server from the cluster
   *
   * @param index
   * @return true
   * @return false
   */
  inline uint64_t remove_server(uint64_t index) {
    if (index >= this->_cache_servers_vec.size()) return false;

    this->_cache_servers_vec.erase(this->_cache_servers_vec.begin() + index);
    this->_server_weights_vec.erase(this->_server_weights_vec.begin() + index);

    // normalize the server weights
    std::vector<double> server_normalized_weights_vec;
    double sum = std::accumulate(this->_server_weights_vec.begin(),
                                 this->_server_weights_vec.end(), 0.0);

    for (auto weight : this->_server_weights_vec) {
      server_normalized_weights_vec.push_back(weight / sum);
    }

    if (this->_ring != nullptr) {
      ch_ring_destroy_ring(this->_ring);
    }

    this->_ring = ch_ring_create_ring(this->_cache_servers_vec.size(),
                                      server_normalized_weights_vec.data());

    return this->_cache_servers_vec.size() - 1;
  }

  /**
   * @brief Get the num server object
   *
   * @return size_t
   */
  inline size_t get_num_server() { return this->_cache_servers_vec.size(); };

  /**
   * @brief set one server unavailable
   *
   * @param server_id
   * @return char
   */
  inline bool fail_one_server(unsigned int server_id) {
    _cache_servers_vec.at(server_id).set_unavailable();
    return _cache_servers_vec.at(server_id).is_available();
  };

  /**
   * @brief recover one server from unavailable state
   *
   * @param server_id
   * @return char
   */
  inline bool recover_one_server(unsigned int server_id) {
    if (!_cache_servers_vec.at(server_id).is_available()) {
      _cache_servers_vec.at(server_id).set_available();
    }
    return _cache_servers_vec.at(server_id).is_available();
  };

  inline unsigned long get_cluster_id() { return this->cluster_id; };

  inline CacheServer &get_server(const unsigned long index) {
    return _cache_servers_vec.at(index);
  };

  ~CacheCluster();
};
}  // namespace CDNSimulator

#endif /* CACHE_CLUSTER_HPP */
