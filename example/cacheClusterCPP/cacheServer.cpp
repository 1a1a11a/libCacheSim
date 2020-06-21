//
//  cacheServer.cpp
//  CDNSimulator
//
//  Created by Juncheng on 11/18/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "cacheServer.hpp"
#include <iostream>

namespace CDNSimulator {

/************************** cacheServer *****************************
 this class is the cache server class, it simulates a cache server
 ********************************************************************/
/**
 *
 * @param server_id
 * @param cache_size    size of this cache server
 * @param cache_alg     the cache eviction algorithm
 * @param data_type     type of input trace type
 * @param params        cache eviction algorithm required parameters
 * @param server_name   name of the server, not used for now
 * @param EC_flag       whether to use EC, this should be true all the time
 * @param EC_n          n used in coding
 * @param EC_k          k used in coding
 */
cacheServer::cacheServer(const unsigned long server_id,
                         const unsigned long cache_size,
                         std::string cache_alg,
                         const unsigned char data_type, const void *params,
                         std::string server_name,
                         const unsigned int EC_n, const unsigned int EC_k)
    : server_stat(server_id, cache_size) {

  this->server_id = server_id;
  this->cache_size = cache_size;
  this->server_name = std::move(server_name);

  this->EC_n = EC_n;
  this->EC_k = EC_k;

  if (cache_alg == "lru") {
    this->cache = LRU_init(this->cache_size, (char) data_type, 0, (void *) params);
  } else if (cache_alg == "fifo") {
    this->cache = fifo_init(this->cache_size, (char) data_type, 0, (void *) params);
  } else if (cache_alg == "lrusize") {
//    this->cache = LRUSize_init(this->cache_size, data_type, 0, (void *) params);

    // this is for tracking eviction age
    char server_ofilename[1024];
    strcpy(server_ofilename, (char*)params);
    strcat(server_ofilename, ("_server"+std::to_string(server_id)).c_str());
//    std::cout << "server output " << std::string(server_ofilename) << std::endl;
    this->cache = LRUSize_init(this->cache_size, (char) data_type, 0, (void *) (server_ofilename));

  } else if (cache_alg == "fifosize") {
    this->cache = FIFOSize_init(this->cache_size, (char) data_type, 0, (void *) params);
  } else {
    ERROR("unknown cache replacement algorithm %s\n", cache_alg.c_str());
    abort();
  }

  srand((unsigned int) time(nullptr));
}

/** use an array of caches to initialize the cache server,
 *  the first cache in the array is layer 1 cache, second is layer 2, etc.
 *  ATTENTION: the life of caches is handed over to cache server,
 *  thus resource de-allocation will be done by cacheServer */
cacheServer::cacheServer(const unsigned long server_id, cache_t *const cache,
                         std::string server_name,
                         const unsigned int EC_n, const unsigned int EC_k)
    : server_stat(server_id, (unsigned long) cache->size) {

  this->server_id = server_id;
  this->server_name = std::move(server_name);
  this->cache = cache;
  this->cache_size = (unsigned long) cache->size;

  this->EC_n = EC_n;
  this->EC_k = EC_k;

  this->server_stat = cacheServerStat(server_id, cache_size);
  srand((unsigned int) time(nullptr));
}

cacheServer::~cacheServer() { this->cache->destroy(this->cache); }
} // namespace CDNSimulator
