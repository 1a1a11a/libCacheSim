//
//  cacheCluster.cpp
//  CDNSimulator
//
//  Created by Juncheng Yang on 7/13/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//

#include "cacheCluster.hpp"
#include <iomanip>

namespace CDNSimulator {

  cacheCluster::cacheCluster(const unsigned long cluster_id,
                             std::string cluster_name,
                             std::string trace_filename,
                             cacheServer **cache_servers, const unsigned long n_server,
                             std::string cache_alg,
                             const unsigned long *server_cache_sizes,
                             const unsigned int admission_limit, bool ICP,
                             const unsigned int EC_n, const unsigned int EC_k,
                             const unsigned int EC_x, bool ghost_parity,
                             const unsigned int EC_size_threshold,
                             std::string trace_type)
    : hasher(n_server),
      cluster_id(cluster_id), cluster_name(std::move(cluster_name)),
      trace_filename(std::move(trace_filename)),
      cache_alg(std::move(cache_alg)),
      trace_type_str(std::move(trace_type)),
      admission_limit(admission_limit), ICP(ICP),
      EC_n(EC_n),
      EC_k(EC_k),
      EC_x(EC_x),
      ghost_parity(ghost_parity),
      EC_size_threshold(EC_size_threshold),
      cluster_stat(cluster_id, n_server, server_cache_sizes, EC_n, EC_k, EC_x) {

    double *weight = new double[n_server];
    unsigned long sum_cache_size = 0;
    for (unsigned long i = 0; i < n_server; i++) {
      sum_cache_size += server_cache_sizes[i];
    }
    for (unsigned long i = 0; i < n_server; i++) {
      weight[i] = (double) server_cache_sizes[i]/(double) sum_cache_size;
    }
    ring = ch_ring_create_ring(int(n_server), weight);
    server_unavailability = (char *) malloc(sizeof(char) * n_server);
    memset(server_unavailability, 0, sizeof(char) * n_server);

    for (unsigned long i = 0; i < n_server; i++) {
//    server_chunkID_map_vec.emplace_back(0);
      this->cache_servers.push_back(cache_servers[i]);
    }

    is_replication = EC_n >= 2 && EC_k == 1;
    chunk_cnt = new int[EC_n];
    server_has_no_chunk = new bool[EC_n];
    checked_servers = new bool[n_server];

    if (!ghost_parity) {
      std::cerr << "do not support no ghost parity" << std::endl;
      abort();
    }

    if (trace_type_str == "akamai1b") {
      this->trace_type = akamai1b;
    } else if (trace_type_str == "akamai1bWithHost") {
      this->trace_type = akamai1bWithHost;
    } else if (trace_type_str == "akamai1bWithBucket") {
      this->trace_type = akamai1bWithBucket;
    } else {
      throw std::invalid_argument("unknown trace type" + trace_type_str);
    }

    srand((unsigned int) time(nullptr));
    delete []weight;
  }

  cluster_hit_t cacheCluster::get(request_t *const req) {

//  unsigned int i;
    size_t i;
    objType obj_type = unknown;

    uint64_t full_obj_size = req->size;
    uint64_t chunk_obj_size = uint64_t(ceil(double(req->size) / EC_k));

    // only support uint32_t label type
    uint32_t obj_key = *(guint64 *) (req->label_ptr);

    this->cluster_stat.cluster_req_cnt++;
    this->cluster_stat.cluster_req_bytes += full_obj_size;
#ifdef TRACK_BUCKET_STAT
    if (trace_type == akamai1bWithBucket) {
      (this->bucket_stat[req->op].req_cnt)++;
      this->bucket_stat[req->op].req_byte += full_obj_size;
    }
#endif
    cluster_hit_t cluster_hit = {false, false, false, 0};

    /* found a chunk or full object in the lead server */
    bool found_in_lead_server;
    /* found in other servers via ICP */
    bool found_via_ICP = false;

    /* whether admit current request into cache if it is a miss */
    bool admit_into_cache = false;
    if (admission_limit <= 0)
      admit_into_cache = true;
    else if (get_freq(req, true) >= admission_limit)
      admit_into_cache = true;


    /** first get the index of the cacheServer which
     *  this request will be sent to using consistent hashing */
    unsigned int server_idxs[EC_n];
    /* either 0 or 1, used to select one of the two lead servers in replication and coding */
    unsigned int idx_selected;
    unsigned int lead_server_idx;

    // now find the lead server
    if (trace_type == akamai1bWithHost && req->op != -1) {
      // op is used to store the idx of selected server in the original akamai edge cluster
      // op field is used to pass in selected host in the original trace, hostID starts with 1
      lead_server_idx = req->op;
      for (i = 0; i < EC_n; i++)
        server_idxs[i] = (lead_server_idx + i) % (cache_servers.size());
    } else {
      // since the trace does not specify node ID, we calculate using consistent hashing
      if (EC_n == 1) {
        // this is not replication, so we always choose the fixed server
        idx_selected = 0;
      } else {
        // select one of the two lead servers that are returned by load balancer
        idx_selected = (unsigned int) rand() % 2;
      }

//    hasher.get_k_idx(req, EC_n, server_idxs);
      if (trace_type == akamai1bWithBucket && req->op != -1) {
//      req->op <<= 1;
        if (req->op == N_BUCKET){
          ERROR("bucket id %ld overflow\n", (long) req->op);
          abort();
        }
        if (((char *) (&(req->op)))[7] != 0)
          throw std::invalid_argument("last bit not 0, bucket " + std::to_string(req->op));

        ch_ring_get_available_servers((const char *) (&(req->op)), ring, EC_n,
                                      server_idxs, server_unavailability);
//        std::cout << "bucket " << req->op << ":" << server_idxs[0] << "," << server_idxs[1]<< "," << server_idxs[2] << "," << server_idxs[3] << std::endl;
      } else {
        ch_ring_get_available_servers((const char *) (req->label_ptr), ring, EC_n,
                                      server_idxs, server_unavailability);
      }
      lead_server_idx = server_idxs[idx_selected];
//      std::cout << "set lead_server_idx2 " << lead_server_idx << " at " << idx_selected << std::endl;
    }

    cluster_hit.req_host = (int) lead_server_idx;
    found_in_lead_server = this->cache_servers.at(lead_server_idx)->check(req);
    if (found_in_lead_server) {
      obj_type = this->cache_servers.at(lead_server_idx)->get_obj_type(req);
    }

    // ALERT: here we assume the label is int, not char*
//  if (obj_type_map.find(obj_key) != obj_type_map.end())
//    obj_type = obj_type_map[obj_key];


//  if (ICP && (!found_in_lead_server) && (obj_type != unknown)) {
    if (ICP && (!found_in_lead_server)) {
      for (i = 0; i < cache_servers.size(); i++) {
        if (i == lead_server_idx)
          continue;
        found_via_ICP = this->cache_servers.at(i)->check(req);
        if (found_via_ICP) {
          obj_type = this->cache_servers.at(i)->get_obj_type(req);
          if (obj_type == full_obj) {
            // update LRU position
            this->cache_servers.at(i)->get(req);
            cluster_stat.intra_cluster_bytes += full_obj_size;
            cluster_hit.ICP_hit = true;
          }
          break;
        }
      }
    }

    /* we have checked lead server and possibly other servers if ICP enabled,
     * if it is a miss, the full object will be fetched from origin */
    if (found_in_lead_server || found_via_ICP) {
      // cache hit, check whether it is full object
      if (obj_type == full_obj) {
        cluster_hit.hit = true;
        this->cache_servers.at(lead_server_idx)->get(req);
        this->cache_servers.at(lead_server_idx)->add_obj_data(req, full_obj, 0);
#ifdef TRACK_EXCLUSIVE_HIT
        if (!this->cache_servers.at(server_idxs[1-idx_selected])->check(req)){
        cluster_stat.exclusive_hit_cnt += 1;
        cluster_stat.exclusive_hit_byte += req->size;
      }
#endif
      } else if (obj_type == chunk_obj) {
        if (!admit_into_cache) {
          ERROR("error happening");
          abort();
        }
        // we find a chunk, now get chunks from the peer caches
        req->size = chunk_obj_size;
        unsigned int chunk_id = 0;
        unsigned int duplicate_chunks = 0;

        for (i = 0; i < EC_n; i++) {
          chunk_cnt[i] = 0;
          server_has_no_chunk[i] = false;
        }

        if (ICP)
          for (i = 0; i < cache_servers.size(); i++)
            checked_servers[i] = false;

        for (i = 0; i < EC_n; i++) {
          if (cache_servers.at(server_idxs[i])->get(req)) {
//          chunk_id = server_chunkID_map_vec.at(server_idxs[i])[obj_key];
            chunk_id = this->cache_servers.at(server_idxs[i])->get_chunk_id(req);
            if (chunk_cnt[chunk_id] == 0) {
              cluster_hit.n_chunk_hit++;
            } else {
              duplicate_chunks++;
            }
            chunk_cnt[chunk_id]++;
          } else {
            server_has_no_chunk[i] = true;
          }
          checked_servers[server_idxs[i]] = true;
        }

        if (ICP) {
          for (i = 0; i < cache_servers.size(); i++) {
            if (checked_servers[i])
              continue;
            if (cache_servers.at(i)->check(req)) {
//            chunk_id = server_chunkID_map_vec.at(i)[obj_key];
              chunk_id = this->cache_servers.at(i)->get_chunk_id(req);
              if (chunk_cnt[chunk_id] == 0) {
                cluster_hit.n_chunk_hit++;
              }
              chunk_cnt[chunk_id]++;
            }
          }
        }

        if ((!ICP) && duplicate_chunks > EC_n - EC_k) {
          std::cerr << "obj " << obj_key << ": find too many (" << duplicate_chunks << ") duplicate chunks, "
                    << cluster_hit.n_chunk_hit << " chunk hit" << std::endl;
          std::cerr << "chunk cnt: " << chunk_cnt[0] << "," << chunk_cnt[1] << "," << chunk_cnt[2] << ","
                    << chunk_cnt[3]
                    << std::endl;
          std::cerr << server_has_no_chunk[0] << "," << server_has_no_chunk[1] << "," << server_has_no_chunk[2] << ","
                    << server_has_no_chunk[3] << std::endl;
          abort();
        }

        // restore all chunks
        for (i = 0; i < EC_n; i++) {
//        server_chunkID_map_vec.at(server_idxs[i])[obj_key] = i;
          this->cache_servers.at(server_idxs[i])->set_chunk_id(req, i);
        }

        cluster_stat.intra_cluster_bytes = duplicate_chunks * chunk_obj_size;

        if (cluster_hit.n_chunk_hit < EC_k) {
          cluster_stat.midgress_bytes += (EC_k - cluster_hit.n_chunk_hit) * chunk_obj_size;
#ifdef TRACK_BUCKET_STAT
          if (trace_type == akamai1bWithBucket)
            this->bucket_stat[req->op].midgress += (EC_k - cluster_hit.n_chunk_hit) * chunk_obj_size;
#endif
        } else {
          cluster_hit.hit = true;
        }
        cluster_stat.intra_cluster_bytes += chunk_obj_size * (EC_n - 1);
        cluster_stat.n_miss[EC_n - cluster_hit.n_chunk_hit]++;

      } else if (obj_type == unknown) {
        ERROR(
          "found obj %ld in cache, but does not find obj_type information, found in lead server %d, found via ICP %d\n",
          (long) *(uint64_t *) req->label_ptr,
          found_in_lead_server,
          found_via_ICP);
        abort();
      }
    } else {
      // object miss, fetch full object from origin and save to n caches
      cluster_stat.midgress_bytes += full_obj_size;
#ifdef TRACK_BUCKET_STAT
      if (trace_type == akamai1bWithBucket)
        this->bucket_stat[req->op].midgress += full_obj_size;
#endif
      if (admit_into_cache) {
//        std::cout << "add " << *(guint64*)(req->label_ptr) << std::endl;
        // this object is not one(n)-hit-wonder
        if (check_coding_policy(req)) {
          // code object
//        obj_type_map[obj_key] = chunk_obj;
          req->size = chunk_obj_size;
          cluster_stat.intra_cluster_bytes += chunk_obj_size * (EC_n - 1);
          // NOTE: C2DN performas splitting without calculating parity chunks at first access
          for (i = 0; i < this->EC_k; i++) {
            this->cache_servers.at(server_idxs[i])->get(req);
            this->cache_servers.at(server_idxs[i])->add_obj_data(req, chunk_obj, i);
//          std::cout << *(guint64*) (req->label_ptr) << ": " << "chunk " << i << std::endl;
//          server_chunkID_map_vec.at(server_idxs[i])[obj_key] = i;
          }
        } else {
//        obj_type_map[obj_key] = full_obj;
          // update cached_obj_data
          this->cache_servers.at(lead_server_idx)->get(req);
          this->cache_servers.at(lead_server_idx)->add_obj_data(req, full_obj, 0);
          unsigned int n_copy = EC_n == EC_k ? EC_n : 1;
          // specifically for (2,2)
          if (n_copy > 1) {
            for (i = 0; i < n_copy; i++) {
              this->cache_servers.at(server_idxs[i])->get(req);
            }
          }
        }
      }
    }

    if (cluster_hit.hit) {
      this->cluster_stat.cluster_hit_cnt++;
      this->cluster_stat.cluster_hit_bytes += full_obj_size;
#ifdef TRACK_BUCKET_STAT
      if (trace_type == akamai1bWithBucket)
        (this->bucket_stat[req->op].hit_cnt)++;
#endif

      if (obj_type == full_obj) {
        cluster_stat.full_obj_hit_cnt += 1;
        cluster_stat.full_obj_hit_bytes += full_obj_size;
      } else if (obj_type == chunk_obj) {
        cluster_stat.chunk_obj_hit_cnt += 1;
        cluster_stat.chunk_obj_hit_bytes += full_obj_size;
      } else {
        ERROR("unknown obj label_type %d", obj_type);
        abort();
      }
    }
    req->size = full_obj_size;
    return cluster_hit;
  }

  void _objmap_aux(gpointer key, gpointer value, gpointer user_data) {
    auto cluster_objmap = (GHashTable *) user_data;
    g_hash_table_insert(cluster_objmap, key, value);
  }

  void _objmap_aux_cpp(gpointer key, gpointer value, gpointer user_data) {
    auto cluster_objmap = (std::unordered_map<uint32_t, unsigned int> *) user_data;
    (*cluster_objmap)[*(guint64 *) key] = 0;
//  std::cout << cluster_objmap << "," << (*cluster_objmap).size() << ": " << (*cluster_objmap)[*(guint64 *)key] << std::endl;
  }


  void _objmap_aux2(gpointer key, gpointer value, gpointer user_data) {
    auto total_size = (guint64 *) user_data;
    guint obj_size = ((cache_obj_t *) ((GList *) value)->data)->size;
    *total_size = *total_size + obj_size;
  }

  void cacheCluster::get_all_cluster_stored_obj() {
    for (size_t i = 0; i < this->get_num_server(); i++) {
      GHashTable *server_objmap = cache_servers.at(i)->cache->get_objmap(cache_servers.at(i)->cache);
      g_hash_table_foreach(server_objmap, _objmap_aux_cpp, (gpointer) &(this->cluster_objmap));
    }
  }

  void cacheCluster::get_all_server_stored_obj() {
    for (size_t i = 0; i < this->get_num_server(); i++) {
      GHashTable *server_objmap = cache_servers.at(i)->cache->get_objmap(cache_servers.at(i)->cache);
      g_hash_table_foreach(server_objmap, _objmap_aux_cpp, (gpointer) &(this->per_server_objmap[i]));
    }
  }

#ifdef FIND_REP_COEF
  /**
   * find the replication factor in 2-rep
   * @param obj replication coefficient in terms of obj
   * @param byte replication coefficient in terms of bytes
   */
  void cacheCluster::find_rep_factor(double *obj, double *byte,
      uint64_t *n_server_obj, uint64_t *n_server_byte, uint64_t *n_cluster_obj, uint64_t *n_cluster_byte) {
    /* first create a giant map to store all the cached objects in the cluster */

    if (cache_servers.at(0)->cache->data_type != OBJ_ID_NUM) {
      ERROR("dataType is not support");
      abort();
    }

    // the sum of each server in the cluster
    uint64_t total_stored_size_servers = 0;
    uint64_t n_total_stored_obj_servers = 0;

    // for the cluster (de-duplication version of the ones above)
    uint64_t total_stored_size_cluster = 0;
    uint64_t n_total_stored_obj_cluster = 0;

    GHashTable *cluster_objmap = g_hash_table_new_full(g_int64_hash, g_int64_equal, nullptr, nullptr);

    for (unsigned int i = 0; i < this->get_num_server(); i++) {
      GHashTable *server_objmap = cache_servers.at(i)->cache->get_objmap(cache_servers.at(i)->cache);
      n_total_stored_obj_servers += g_hash_table_size(server_objmap);
      total_stored_size_servers += cache_servers.at(i)->cache->get_current_size(cache_servers.at(i)->cache);

      g_hash_table_foreach(server_objmap, _objmap_aux, (gpointer) cluster_objmap);
    }
    n_total_stored_obj_cluster = g_hash_table_size(cluster_objmap);
    g_hash_table_foreach(cluster_objmap, _objmap_aux2, &total_stored_size_cluster);

    *n_server_obj = n_total_stored_obj_servers;
    *n_server_byte = total_stored_size_servers;
    *n_cluster_obj = n_total_stored_obj_cluster;
    *n_cluster_byte = total_stored_size_cluster;

    *obj = (double) n_total_stored_obj_servers / (double) n_total_stored_obj_cluster;
    *byte = (double) total_stored_size_servers / (double) total_stored_size_cluster;
    g_hash_table_destroy(cluster_objmap);
  }
#endif


#ifdef FIND_REP_COEF_AGE
  void _objmap_aux_age(gpointer key, gpointer value, gpointer user_data) {
    rep_factor_iter_data_t* rep_factor_data = (rep_factor_iter_data_t*) user_data;
    guint64 age = rep_factor_data->current_time - ((cache_obj_t *) ((GList *) value)->data)->access_time;

    if (age < rep_factor_data->max_age){
      rep_factor_data->n_req_age[age] += 1;

      if (g_hash_table_contains(rep_factor_data->prev_server, key) || g_hash_table_contains(rep_factor_data->next_server, key)){
        rep_factor_data->n_rep_age[age] += 1;
      }

      if (rep_factor_data->n_req_age[age] < rep_factor_data->n_rep_age[age]){
        std::cerr << "age " << age << ": " << rep_factor_data->n_req_age[age] << ", " << rep_factor_data->n_rep_age[age] << std::endl;
      }
    }
  }


  void cacheCluster::find_rep_factor_vs_age(uint64_t max_age, uint16_t *n_req_age, uint16_t *n_rep_age) {
    /* first create a giant map to store all the cached objects in the cluster */

    if (cache_servers.at(0)->cache->data_type != OBJ_ID_NUM) {
      ERROR("dataType is not supported");
      abort();
    }

    GHashTable *cluster_objmap = g_hash_table_new_full(g_int64_hash, g_int64_equal, nullptr, nullptr);
    rep_factor_iter_data_t rep_factor_data;

    rep_factor_data.max_age = max_age;
    rep_factor_data.n_req_age = n_req_age;
    rep_factor_data.n_rep_age = n_rep_age;

    for (unsigned int i = 0; i < this->get_num_server(); i++) {
      GHashTable *server_objmap = cache_servers.at(i)->cache->get_objmap(cache_servers.at(i)->cache);
      g_hash_table_foreach(server_objmap, _objmap_aux, (gpointer) cluster_objmap);
    }

    for (unsigned int i = 0; i < this->get_num_server(); i++) {
      GHashTable *server_objmap = cache_servers.at(i)->cache->get_objmap(cache_servers.at(i)->cache);

      rep_factor_data.current_time = cache_servers.at(i)->cache->ts;
      rep_factor_data.prev_server = cache_servers.at(i)->cache->get_objmap(cache_servers.at((i-1)%this->get_num_server())->cache);
      rep_factor_data.next_server = cache_servers.at(i)->cache->get_objmap(cache_servers.at((i+1)%this->get_num_server())->cache);

      g_hash_table_foreach(server_objmap, _objmap_aux_age, (gpointer) &rep_factor_data);
    }
  }
#endif

#ifdef FIND_REP_COEF_POPULARITY
  // not finished
  void _objmap_aux_popularity(gpointer key, gpointer value, gpointer user_data) {
    rep_factor_iter_data_t* rep_factor_data = (rep_factor_iter_data_t*) user_data;
    guint64 age = rep_factor_data->current_time - ((cache_obj_t *) ((GList *) value)->data)->access_time;

    if (age < rep_factor_data->max_age){
      rep_factor_data->n_req_age[age] += 1;

      if (g_hash_table_contains(rep_factor_data->prev_server, key) || g_hash_table_contains(rep_factor_data->next_server, key)){
        rep_factor_data->n_rep_age[age] += 1;
      }

      if (rep_factor_data->n_req_age[age] < rep_factor_data->n_rep_age[age]){
        std::cerr << "age " << age << ": " << rep_factor_data->n_req_age[age] << ", " << rep_factor_data->n_rep_age[age] << std::endl;
      }
    }
  }


  void cacheCluster::find_rep_factor_vs_popularity(uint64_t max_popularity, uint16_t *n_req_popularity, uint16_t *n_rep_popularity) {
    /* first create a giant map to store all the cached objects in the cluster */

    if (cache_servers.at(0)->cache->data_type != OBJ_ID_NUM) {
      ERROR("dataType is not supported");
      abort();
    }

    GHashTable *cluster_objmap = g_hash_table_new_full(g_int64_hash, g_int64_equal, nullptr, nullptr);
    rep_factor_iter_data_t rep_factor_data;

    rep_factor_data.max_popularity = max_popularity;
    rep_factor_data.n_req_popularity = n_req_popularity;
    rep_factor_data.n_rep_popularity = n_rep_popularity;

    for (unsigned int i = 0; i < this->get_num_server(); i++) {
      GHashTable *server_objmap = cache_servers.at(i)->cache->get_objmap(cache_servers.at(i)->cache);
      g_hash_table_foreach(server_objmap, _objmap_aux, (gpointer) cluster_objmap);
    }

    for (unsigned int i = 0; i < this->get_num_server(); i++) {
      GHashTable *server_objmap = cache_servers.at(i)->cache->get_objmap(cache_servers.at(i)->cache);

      rep_factor_data.current_time = cache_servers.at(i)->cache->ts;
      rep_factor_data.prev_server = cache_servers.at(i)->cache->get_objmap(cache_servers.at((i+9)%this->get_num_server())->cache);
      rep_factor_data.next_server = cache_servers.at(i)->cache->get_objmap(cache_servers.at((i+1)%this->get_num_server())->cache);

      g_hash_table_foreach(server_objmap, _objmap_aux_popularity, (gpointer) &rep_factor_data);
    }
  }
#endif

  bool cacheCluster::add_server(cacheServer *cache_server) {
    ERROR("not implemented\n");
    abort();
  }

  bool cacheCluster::remove_server(unsigned long index) {
    ERROR("not implemented\n");
    abort();
  }

  cacheCluster::~cacheCluster() {
    delete[] chunk_cnt;
    free(server_unavailability);
    ch_ring_destroy_ring(ring);
  }
} // namespace CDNSimulator
