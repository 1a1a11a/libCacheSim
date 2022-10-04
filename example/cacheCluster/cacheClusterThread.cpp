//
//  cacheClusterThread.cpp
//  CDNSimulator
//
//  Created by Juncheng Yang on 11/20/18.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "cacheClusterThread.hpp"
#include <exception>

using namespace CDNSimulator;

void cacheClusterThread::run() {
  request_t *req = new_req_struct();
  req->label_type = reader->base->label_type;
  log_ofstream << "#" << this->param_string << std::endl;
  log_ofstream << "#vtime, " << cacheClusterStat::stat_str_header(false);

  read_one_element(reader, req);
  if (req->size <= 0) {
    std::cerr << "request size 0" << std::endl;
    req->size = 100 * 1024;
  }
  // std::cout << "host " << req->op << std::endl;

  cache_start_time = req->real_time;
  last_cluster_update = req->real_time;
  last_log_otime = 0;
  last_log_otime = req->real_time;
  cluster_hit_t hit_result;
  INFO("cache cluster started, trace start time %ld\n", cache_start_time);

#ifdef OUTPUT_HIT_RESULT
  bool hit;
#endif

#ifdef TRACK_FAILURE_IMPACTED_HIT_START
  uint32_t objkey;
  uint64_t tracking_start_rtime = 0;
  uint64_t last_tracking_log_time = 0;
#endif

#ifdef FIND_REP_COEF
  uint64_t last_find_rep_coef_time = req->real_time;
#endif

  while (req->valid) {
    if (!failure_vector_deque.empty()) {
      // update server availability info
      if (req->real_time - last_cluster_update >= cluster_update_interval) {
        for (unsigned long i = 0; i < cache_cluster->get_num_server(); i++)
          cache_cluster->recover_one_server(i);

        std::vector<int> failed_servers = failure_vector_deque.front();
#ifdef DEBUG_UNAVAILABILITY
        std::cout << req->real_time << "," << vtime << "," << "update failed servers ";
        std::for_each(failed_servers.begin(),
                      failed_servers.end(),
                      [](const int e) { std::cout << e << " "; });
        std::cout << std::endl;
#endif

        for (auto &it: failed_servers) {
          if (cache_cluster->fail_one_server(it) != 0) {
            std::cerr << "fail to create server failure" << std::endl;
          }
        }
        failure_vector_deque.pop_front();
        last_cluster_update = req->real_time;
      }
    }

    vtime++;

    if (req->size <= 0) {
      // this is for the synthetic traces with no req size
      req->size = 100 * 1024;
    }
    hit_result = cache_cluster->get(req);

#ifdef OUTPUT_HIT_RESULT
    char hit_char = hit_result.hit?'1':'0';
    hit_result_ofstream.write(&hit_char, sizeof(hit_char));
#endif

#ifdef TRACK_FAILURE_IMPACTED_HIT_START
    if (vtime == TRACK_FAILURE_IMPACTED_HIT_START) {
      tracking_start_rtime = req->real_time;
      last_tracking_log_time = req->real_time;
      this->cache_cluster->get_all_server_stored_obj();
      failure_impacted_hit_ofstream << "# freeze cluster obj cache, total ";

      for (size_t i = 0; i < 10; i++) {
        failure_impacted_hit_ofstream << this->cache_cluster->per_server_objmap[i].size() << ",";
        req_cnt[i] = 0;
        req_byte[i] = 0;
        hit_cnt[i] = 0;
        hit_byte[i] = 0;
      }
      failure_impacted_hit_ofstream << " obj" << std::endl;
    }
    if (vtime > TRACK_FAILURE_IMPACTED_HIT_START) {
      req_cnt[hit_result.req_host]++;
      req_byte[hit_result.req_host] += req->size;
      if (hit_result.hit) {
        objkey = *(guint64 *) (req->label_ptr);
        auto &objmap = this->cache_cluster->per_server_objmap[hit_result.req_host];
        if (objmap.find(objkey) != objmap.end() && objmap[objkey] == 0) {
          hit_cnt[hit_result.req_host]++;
          hit_byte[hit_result.req_host] += req->size;
          objmap[objkey] = 1;
        }
      }

      if (req->real_time != last_tracking_log_time && (req->real_time - tracking_start_rtime) % log_interval == 0) {
        last_tracking_log_time = req->real_time;
        failure_impacted_hit_ofstream << req->real_time-tracking_start_rtime << "," << vtime << ",";
        for (size_t i = 0; i < 10; i++) {
          failure_impacted_hit_ofstream << hit_cnt[i] << "/" << req_cnt[i] << "/" << hit_byte[i] << "/" << req_byte[i];
          if (i != 9) failure_impacted_hit_ofstream << ","; else failure_impacted_hit_ofstream << std::endl;
          req_cnt[i] = 0;
          req_byte[i] = 0;
          hit_cnt[i] = 0;
          hit_byte[i] = 0;
        }
      }
    }

#endif

    if (log_interval != 0 && req->real_time - last_log_otime >= log_interval) {
//    if (log_interval != 0 && vtime - last_log_otime >= log_interval) {
//      if (vtime % (20 * log_interval) == 0) {
//      INFO("finish request %ld %ld - %lld - %ld\n", (long) vtime, (long) (req->real_time),
//           (long long) (*(guint64 *) (req->label_ptr)), (long) (req->size));
//      }
//      last_log_otime = vtime;
//      log_ofstream << vtime << ", " << cluster_stat->stat_str(false, false);

      last_log_otime = req->real_time;
      log_ofstream << req->real_time << ", " << cluster_stat->stat_str(false, false);

#ifdef TRACK_BUCKET_STAT
      log_bucket_ofstream << req->real_time << "," << vtime << ",";
      for (size_t i=0; i<N_BUCKET-1; i++){
        log_bucket_ofstream << cache_cluster->bucket_stat[i].stat_str() << ",";
        cache_cluster->bucket_stat[i].clear();
      }
      log_bucket_ofstream << cache_cluster->bucket_stat[N_BUCKET-1].stat_str() << std::endl;
      cache_cluster->bucket_stat[N_BUCKET-1].clear();

//      std::cout << "bucket 40 " << cache_cluster->bucket_stat[7]
#endif
    }

#ifdef FIND_REP_COEF
    if (log_interval != 0 && req->real_time - last_find_rep_coef_time >= log_interval * 20 ) {
      last_find_rep_coef_time = req->real_time;
      uint64_t n_server_obj, n_server_byte, n_cluster_obj, n_cluster_byte;
      cache_cluster->find_rep_factor(&cluster_stat->rep_coef_obj, &cluster_stat->rep_coef_byte,
                                     &n_server_obj, &n_server_byte, &n_cluster_obj, &n_cluster_byte);
      rep_coef_ofstream << vtime << ", " << cluster_stat->rep_coef_obj << "/" << cluster_stat->rep_coef_byte
                        << ", " << n_server_obj << "/" << n_cluster_obj << ", " << n_server_byte << "/"
                        << n_cluster_byte << std::endl;
    }
#endif

    read_one_element(reader, req);
//    while (req->size == 0) {
//      std::cerr << "req " << *(uint64_t *) req->label_ptr << " size 0" << std::endl;
//      read_one_element(reader, req);
//    }
  }

#ifdef FIND_REP_COEF_AGE
  uint64_t max_age = 2000000;
  uint16_t *n_req_age = new uint16_t[max_age]();
  uint16_t *n_rep_age = new uint16_t[max_age]();

  cache_cluster->find_rep_factor_vs_age(max_age, n_req_age, n_rep_age);
  std::ofstream rep_coef_ofstream;
  rep_coef_ofstream.open(ofilename+".rep_coef", std::ofstream::out | std::ofstream::trunc);
  rep_coef_ofstream.precision(4);

  for (unsigned int i = 0; i<max_age; i++){
    if (n_req_age[i] != 0)
      rep_coef_ofstream << n_rep_age[i] << "," << n_req_age[i] << "," << (double) n_rep_age[i]/ (double) n_req_age[i] << std::endl;
    else
      rep_coef_ofstream << 0 << "," << 0 << "," << 0 << std::endl;
  }
  delete[] n_req_age;
  delete[] n_rep_age;

#endif

#ifdef FIND_REP_COEF_Poularity

  // dump cache for offline analysis
  for (unsigned int i = 0; i < this->get_num_server(); i++) {
    GHashTable *server_objmap = cache_servers.at(i)->cache->get_objmap(cache_servers.at(i)->cache);
    g_hash_table_foreach(server_objmap, _objmap_aux, (gpointer) cluster_objmap);
  }

  /**
  uint64_t max_popularity = 200000;
  uint16_t *n_req_popularity = new uint16_t[max_popularity]();
  uint16_t *n_rep_popularity = new uint16_t[max_popularity]();

  cache_cluster->find_rep_factor_vs_popularity(max_popularity, n_req_popularity, n_rep_popularity);

  std::ofstream rep_coef_ofstream;
  rep_coef_ofstream.open(ofilename+".rep_coef", std::ofstream::out | std::ofstream::trunc);
  rep_coef_ofstream.precision(4);

  for (unsigned int i = 0; i<max_popularity; i++){
    if (n_req_popularity[i] != 0)
      rep_coef_ofstream << n_rep_popularity[i] << "," << n_req_popularity[i] << "," << (double) n_rep_popularity[i]/ (double) n_req_popularity[i] << std::endl;
    else
      rep_coef_ofstream << 0 << "," << 0 << "," << 0 << std::endl;
  }
  delete[] n_req_popularity;
  delete[] n_rep_popularity;
  **/
#endif

  destroy_req_struct(req);
  log_ofstream << cluster_stat->
      final_stat()
               <<
               std::endl;
  log_ofstream.
      close();
#ifdef TRACK_BUCKET_STAT
  log_bucket_ofstream.close();
#endif
}
//} // namespace CDNSimulator
