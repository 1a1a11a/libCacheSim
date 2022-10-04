//
//  cacheCluster.hpp
//  CDNSimulator
//
//  Created by Juncheng Yang on 11/18/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifndef CACHE_CLUSTER_HPP
#define CACHE_CLUSTER_HPP

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "cache.h"
#include "cacheHeader.h"
#include "logging.h"
#include "reader.h"

#ifdef __cplusplus
}
#endif

#include "cacheServer.hpp"
#include "constCDNSimulator.hpp"
#include "hasher.hpp"
#include "stat.hpp"
#include "consistentHash.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <exception>
#include <stdexcept>
#include <unordered_map>
#include <vector>


#undef RECORD_SERVER_STAT

namespace CDNSimulator {

typedef struct {
  bool hit;
  bool ICP_hit;
  bool RAM_hit;
  unsigned int n_chunk_hit;
  int req_host;
} cluster_hit_t;

typedef struct {
  GHashTable *prev_server;
  GHashTable *next_server;
  uint64_t max_age;
  uint16_t *n_req_age;
  uint16_t *n_rep_age;
  guint64 current_time;
} rep_factor_iter_data_t;


class cacheCluster {
  myHasher hasher;
  ring_t *ring;

 public:
  unsigned long cluster_id;
  std::string cluster_name;
  std::string trace_filename;
  std::string cache_alg;
  std::string trace_type_str;
  trace_type_t trace_type;

  // filter out n-hit-wonders
  unsigned int admission_limit;

  // this only works on unsigned int key type
  std::unordered_map<uint32_t, unsigned int> obj_freq_map;
//  std::unordered_map<uint32_t, objType> obj_type_map;
//  std::vector<std::unordered_map<uint32_t, unsigned char>> server_chunkID_map_vec;

  bool ICP = false;
  // when simulating ICP, we don't want to check all servers due to efficiency
  // instead we only check n_ICP_check_server servers, note that this does not include the head server
//  unsigned int n_ICP_check_server = 2;
  char *server_unavailability;

  bool is_replication = true;
  int *chunk_cnt;
  bool *server_has_no_chunk;
  bool *checked_servers; // used in ICP only

  unsigned int EC_n;
  unsigned int EC_k;
  unsigned int EC_x;
  bool ghost_parity;
  unsigned int EC_size_threshold;

  // for tracking unavailability affected hit
  std::unordered_map<uint32_t, unsigned int> cluster_objmap; // mapping from objID to 0
  std::unordered_map<uint32_t, unsigned int> per_server_objmap[10]; // mapping from objID to 0
  void get_all_cluster_stored_obj();
  void get_all_server_stored_obj();

  std::vector<cacheServer *> cache_servers;
  cacheClusterStat cluster_stat;
  bucketStat bucket_stat[N_BUCKET];

  cacheCluster(const unsigned long cluster_id,
               std::string cluster_name,
               std::string trace_filename,
               cacheServer **cache_servers,
               const unsigned long n_server, std::string cache_alg,
               const unsigned long *server_cache_sizes,
               const unsigned int admission_limit, const bool ICP,
               const unsigned int EC_n, const unsigned int EC_k,
               const unsigned int EC_x = 0, bool ghost_parity = true,
               const unsigned int EC_size_threshold = 0,
               std::string trace_type = "");

  /** this is used to add request to current cluster **/
  cluster_hit_t get(request_t *cp);

  inline unsigned int get_freq(request_t *cp, bool inc) {
    //  std::string key = std::string((char *)(cp->label_ptr));
    uint32_t key = *(guint64 *) (cp->label_ptr);
    unsigned int freq = 0;
    if (obj_freq_map.find(key) != obj_freq_map.end())
      freq = obj_freq_map[key];

    if (inc)
      obj_freq_map[key] = freq + 1;

    return freq;
  }

  inline size_t get_num_server() { return this->cache_servers.size(); };

  inline char fail_one_server(unsigned int server_id) {
    cache_servers.at(server_id)->set_unavailable();
    server_unavailability[server_id] = 1;
    cluster_stat.n_avail_server--;
    return cache_servers.at(server_id)->is_available();
  };

  inline char recover_one_server(unsigned int server_id) {
    if (!cache_servers.at(server_id)->is_available()){
      cache_servers.at(server_id)->set_available();
      server_unavailability[server_id] = 0;
      cluster_stat.n_avail_server++;
    }
    return cache_servers.at(server_id)->is_available();
  };

  inline unsigned long get_cluster_id() { return this->cluster_id; };

  inline cacheClusterStat *get_cluster_stat() { return &this->cluster_stat; }

  inline cacheServer *get_server(const unsigned long index) {
    return cache_servers.at(index);
  };

  inline bool check_coding_policy(request_t *cp) {
    return (!is_replication) && (EC_n != 1) && cp->size > EC_size_threshold;
  }

#ifdef FIND_REP_COEF
  void find_rep_factor(double *obj, double *byte,
      uint64_t *n_server_obj, uint64_t *n_server_byte,
      uint64_t *n_cluster_obj, uint64_t *n_cluster_byte);
#endif

#ifdef FIND_REP_COEF_AGE
  void find_rep_factor_vs_age(uint64_t max_age, uint16_t *n_req_age, uint16_t *n_rep_age);
#endif

  bool add_server(cacheServer *cache_server);

  bool remove_server(unsigned long index);

  ~cacheCluster();
};
} // namespace CDNSimulator

#endif /* CACHE_CLUSTER_HPP */
