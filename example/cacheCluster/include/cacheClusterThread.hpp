//
//  cacheClusterThread.hpp
//  CDNSimulator
//
//  Created by Juncheng Yang on 11/20/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#ifndef CACHE_CLUSTER_THREAD_hpp
#define CACHE_CLUSTER_THREAD_hpp

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cache.h"
#include "cacheHeader.h"
#include "logging.h"
#include "reader.h"

#ifdef __cplusplus
}
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <boost/algorithm/string.hpp>


#include "cacheCluster.hpp"
#include "constCDNSimulator.hpp"

namespace CDNSimulator {

class cp_comparator {
 public:
  bool operator()(request_t *a, request_t *b) {
    return (a->real_time > b->real_time);
  }
};

class cacheClusterThread {

  std::string param_string;
  reader_t *reader;

  std::string failure_data_file;
  std::deque<std::vector<int>> failure_vector_deque;

  cacheCluster *cache_cluster;
  cacheClusterStat *cluster_stat;

  unsigned long cache_start_time = 0;
  unsigned long last_cluster_update;
  unsigned long cluster_update_interval = 5 * 60;
//  unsigned long cluster_update_interval = 20;
  unsigned long vtime;
  unsigned long log_interval;

  std::string log_folder;
  std::string ofilename;
  std::ofstream log_ofstream;
  unsigned long last_log_otime;

#ifdef FIND_REP_COEF
  std::ofstream rep_coef_ofstream;
#endif

#ifdef TRACK_FAILURE_IMPACTED_HIT_START
  uint64_t hit_cnt[10], hit_byte[10];
  uint64_t req_cnt[10], req_byte[10];
  std::ofstream failure_impacted_hit_ofstream;
#endif

#ifdef OUTPUT_HIT_RESULT
  std::ofstream hit_result_ofstream;
#endif

#ifdef TRACK_BUCKET_STAT
  std::ofstream log_bucket_ofstream;
#endif

 public:
  cacheClusterThread(std::string param_string, cacheCluster *cache_cluster_in,
                     reader_t *const reader,
                     std::string failure_data_file,
                     std::string log_folder,
                     unsigned long log_interval = 20000)
      : param_string(std::move(param_string)),
        reader(reader),
        failure_data_file(failure_data_file),
        cache_cluster(cache_cluster_in),
        cluster_stat(&cache_cluster->cluster_stat), vtime(0),
        log_interval(log_interval), log_folder(std::move(log_folder)),
        log_ofstream(), last_log_otime(0) {
    ofilename =
        this->log_folder + "/cluster" +
            cache_cluster->cluster_name + "_" +
            cache_cluster->trace_filename + "_" +
            cache_cluster->cache_alg + "_" +
            std::to_string(cluster_stat->cluster_cache_size_in_gb) + "GB" + "_m" +
//            std::to_string(cluster_stat->cluster_cache_size_in_mb) + "MB" + "_m" +
            std::to_string(cluster_stat->n_server) + "_o" +
            std::to_string(cache_cluster->admission_limit) + "_n" +
            std::to_string(cache_cluster->EC_n) + "_k" +
            std::to_string(cache_cluster->EC_k) + "_z" +
            std::to_string(cache_cluster->EC_size_threshold);

    if (cache_cluster->ghost_parity)
      ofilename += "_g";

    if (cache_cluster->ICP)
      ofilename += "_icp";

    log_ofstream.open(ofilename, std::ofstream::out | std::ofstream::trunc);

    if (!failure_data_file.empty()) {
      std::string line;
      std::ifstream failure_data_ifstream;
      failure_data_ifstream.open(failure_data_file);
      while (std::getline(failure_data_ifstream, line)) {
        std::vector<std::string> line_split;
        std::vector<int> failed_servers;

        if (!line.empty()) {
          boost::split(line_split, line, [](char c) { return c == ' '; });
          for (auto &it: line_split) {
//          std::cout << it << std::endl;
            failed_servers.push_back(std::stoi(it));
          }
        }
        failure_vector_deque.push_back(failed_servers);
//        std::cout << ", " << failed_servers.size() << ", " << failure_vector_deque.size() << std::endl;
      }
      failure_data_ifstream.close();
      INFO("load failure data %lu pts\n", failure_vector_deque.size());
    }

#ifdef FIND_REP_COEF
    if (cache_cluster->EC_n !=2){
      ERROR("find replication only works on two-replication");
      abort();
    }
    rep_coef_ofstream.open(ofilename+".rep_coef", std::ofstream::out | std::ofstream::trunc);
    rep_coef_ofstream.precision(4);
#endif

#ifdef TRACK_FAILURE_IMPACTED_HIT_START
    failure_impacted_hit_ofstream.open(ofilename+".failureHit", std::ofstream::out | std::ofstream::trunc);
    failure_impacted_hit_ofstream.precision(4);
#endif

#ifdef OUTPUT_HIT_RESULT
    hit_result_ofstream.open(ofilename+".hit", std::ofstream::binary | std::ofstream::out | std::ofstream::trunc);
#endif

#ifdef TRACK_BUCKET_STAT
    log_bucket_ofstream.open(ofilename + ".bucket", std::ofstream::out | std::ofstream::trunc);
#endif
  };

  void run();

  cacheCluster *get_cache_cluster() { return this->cache_cluster; }
  cacheClusterStat *get_cluster_stat() { return this->cluster_stat; }
};

} // namespace CDNSimulator

#endif /* CACHE_CLUSTER_THREAD_hpp */
