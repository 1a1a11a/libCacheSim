//
//  cacheServer.cpp
//  CDNSimulator
//
//  Created by Juncheng on 7/11/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//

#ifndef CACHE_SERVER_HPP
#define CACHE_SERVER_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include "FIFO.h"
#include "FIFOSize.h"
#include "LRU.h"
#include "LRUSize.h"
#include "cache.h"
#include "cacheHeader.h"
#include "logging.h"
#include "splay.h"
#include <glib.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>


#include "constCDNSimulator.hpp"
#include "stat.hpp"

namespace CDNSimulator {


typedef struct {
  unsigned int obj_type: 4;
  unsigned int chunk_id: 14;
} cached_obj_data_t;


class cacheServer {

 public:
  cache_t *cache;

  std::string server_name;
  unsigned long server_id;
  unsigned long cache_size;

  /* availability simulation */
  bool available = true;

  /* related to erasure coding */
  unsigned int EC_n = 0;
  unsigned int EC_k = 0;

  cacheServerStat server_stat;

#ifdef TRACK_BYTE_REUSE
  std::unordered_map<uint64_t, uint64_t> obj_byte;
  std::unordered_map<uint64_t, uint64_t> obj_reuse_byte;
#endif

  cacheServer(unsigned long server_id, unsigned long cache_size,
              std::string cache_alg, unsigned char data_type,
              const void *params = nullptr,
              std::string server_name = "default server", unsigned int EC_n = 0,
              unsigned int EC_k = 0);

  cacheServer(unsigned long server_id, cache_t *cache,
              std::string server_name = "default server", unsigned int EC_n = 0,
              unsigned int EC_k = 0);



  inline bool get(const request_t *req) {
    if (!available)
      return false;

    bool hit = (bool) this->cache->add_element(this->cache, (request_t *) req);
//    hit = (bool) (this->cache->check_element(this->cache, (request_t *) req));
//    std::cout << "find " << *(guint64*) (req->label_ptr) << ": " << hit << std::endl;
    return hit;
  }

  inline bool check(const request_t *req) {
    if (!available)
      return false;

    bool hit = (bool) (this->cache->check_element(this->cache, (request_t *) req));
    return hit;
  }

  inline objType get_obj_type(const request_t* const req){
    return (objType) ((cached_obj_data_t*)(this->cache->get_cached_data(this->cache, (request_t *) req)))->obj_type;
  }

  inline void set_obj_type(const request_t* const req, objType obj_type){
    if (this->cache->get_cached_data(this->cache, (request_t *) req) == nullptr) {
      cached_obj_data_t *cached_obj_data = g_new(cached_obj_data_t, 1);
      cached_obj_data->obj_type = obj_type;
      this->cache->update_cached_data(this->cache, (request_t *) req, cached_obj_data);
    } else {
      ((cached_obj_data_t*)(this->cache->get_cached_data(this->cache, (request_t *) req)))->obj_type = (unsigned int) obj_type;
    }
  }

  inline unsigned int get_chunk_id(const request_t* const req) {
//    std::cout << "cached data " << this->cache->get_cached_data(this->cache, (request_t *) req) << std::endl;
    return ((cached_obj_data_t *) (this->cache->get_cached_data(this->cache, (request_t *) req)))->chunk_id;
  }

  inline void set_chunk_id(const request_t* const req, unsigned int chunk_id) {
    if (this->cache->get_cached_data(this->cache, (request_t *) req) == nullptr) {
      cached_obj_data_t *cached_obj_data = g_new(cached_obj_data_t, 1);
      cached_obj_data->obj_type = chunk_obj;
      cached_obj_data->chunk_id = chunk_id;
      this->cache->update_cached_data(this->cache, (request_t *) req, cached_obj_data);
    } else {
      ((cached_obj_data_t *) (this->cache->get_cached_data(this->cache, (request_t *) req)))->chunk_id = chunk_id;
    }
  }

  inline void add_obj_data(const request_t* const req, objType obj_type, unsigned int chunk_id) {
    cached_obj_data_t *cached_obj_data = g_new(cached_obj_data_t, 1);
    cached_obj_data->obj_type = obj_type;
    cached_obj_data->chunk_id = chunk_id;
    this->cache->update_cached_data(this->cache, (request_t *) req, cached_obj_data);
  }

  inline bool set_unavailable(){ available = false; return available; }

  inline bool set_available() {available = true; return available; }

  inline bool is_available() { return available; }

  inline unsigned long get_server_id() { return this->server_id; }

  ~cacheServer();
};

} // namespace CDNSimulator

#endif /* CACHE_SERVER_HPP */
