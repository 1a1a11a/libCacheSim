
#include "lifetime.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../include/libCacheSim/reader.h"
#include "../utils/include/utils.h"

namespace traceAnalyzer {
using namespace std;

void LifetimeDistribution::add_req(request_t *req) {
  n_req_++;
  // printf("%ld\n", req->clock_time);
  auto itr = first_last_req_clock_time_map_.find(req->obj_id);
  if (itr == first_last_req_clock_time_map_.end()) {
    if (req->clock_time < object_create_clock_time_max_) {
      first_last_req_clock_time_map_[req->obj_id] =
          pair<int64_t, int64_t>(req->clock_time, req->clock_time);
      first_last_req_logical_time_map_[req->obj_id] =
          pair<int64_t, int64_t>(n_req_, n_req_);
    }
  } else {
    itr->second.second = req->clock_time;
    auto itr2 = first_last_req_logical_time_map_.find(req->obj_id);
    itr2->second.second = n_req_;
  }
}

void LifetimeDistribution::dump(string &path_base) {
  unordered_map<int64_t, int64_t> clock_lifetime_distribution;
  for (auto &p : first_last_req_clock_time_map_) {
    int64_t lifetime = ((int64_t)(p.second.second - p.second.first) / 10) * 60;
    auto itr = clock_lifetime_distribution.find(lifetime);
    if (itr == clock_lifetime_distribution.end()) {
      clock_lifetime_distribution[lifetime] = 1;
    } else {
      itr->second++;
    }
  }
  vector<pair<int64_t, int64_t>> clock_lifetime_distribution_vec;
  for (auto &p : clock_lifetime_distribution) {
    clock_lifetime_distribution_vec.push_back(p);
  }
  sort(clock_lifetime_distribution_vec.begin(),
       clock_lifetime_distribution_vec.end(),
       [](const pair<int64_t, int64_t> &a, const pair<int64_t, int64_t> &b) {
         return a.first < b.first;
       });

  ofstream ofs(path_base + ".lifetime", ios::out | ios::trunc);
  ofs << "# " << path_base << "\n";
  ofs << "# lifetime distribution (clock) \n";
  for (auto &p : clock_lifetime_distribution_vec) {
    ofs << p.first << ":" << p.second << "\n";
  }

  unordered_map<int64_t, int64_t> logical_lifetime_distribution;
  for (auto &p : first_last_req_logical_time_map_) {
    int64_t lifetime =
        ((int64_t)(p.second.second - p.second.first) / 100) * 100;
    auto itr = logical_lifetime_distribution.find(lifetime);
    if (itr == logical_lifetime_distribution.end()) {
      logical_lifetime_distribution[lifetime] = 1;
    } else {
      itr->second++;
    }
  }
  vector<pair<int64_t, int64_t>> logical_lifetime_distribution_vec;
  for (auto &p : logical_lifetime_distribution) {
    logical_lifetime_distribution_vec.push_back(p);
  }
  sort(logical_lifetime_distribution_vec.begin(),
       logical_lifetime_distribution_vec.end(),
       [](const pair<int64_t, int64_t> &a, const pair<int64_t, int64_t> &b) {
         return a.first < b.first;
       });

  ofs << "# lifetime distribution (logical) \n";
  for (auto &p : logical_lifetime_distribution_vec) {
    ofs << p.first << ":" << p.second << "\n";
  }
  ofs.close();
}
};  // namespace traceAnalyzer
