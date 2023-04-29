#pragma once

/**
 * sample specified number of objects and record their accesses
 * we can plot the access patterns and this can show scan-type requests
 *
 *
 * it does not use an adaptive object selection to sample a given number of
 * objects, because changing the sample ratio at different time will cause
 * a bias when we plot the access pattern
 * so we use a static sample ratio
 *
 */

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../include/libCacheSim/logging.h"
#include "struct.h"

using namespace std;

namespace analysis {
class AccessPattern {
 public:
  /**
   *
   * @param n_req the total number of requests in the trace
   * @param n_obj the number of objects we would like to sample
   */
  explicit AccessPattern(int64_t n_req, int sample_ratio = 1001)
      : sample_ratio_(sample_ratio), n_total_req_(n_req) {
    if (n_total_req_ > 0xfffffff0) {
      INFO(
          "trace is too long (more than 0xfffffff0 requests), accessPattern "
          "will be limited to 0xfffffff0 requests\n");
    }

    if (sample_ratio_ < 1) {
      ERROR(
          "sample_ratio samples 1/sample_ratio objects, and should be at least "
          "1 (no sampling), current value: %d\n",
          sample_ratio_);
      sample_ratio_ = 1;
    }
  };

  ~AccessPattern() = default;

  friend ostream &operator<<(ostream &os, const AccessPattern &pattern) {
    return os;
  }

  void add_req(const request_t *req) {
    if (n_seen_req_ > 0xfffffff0) {
      return;
    }
    n_seen_req_ += 1;

    if (start_rtime_ == -1) {
      start_rtime_ = req->clock_time;
    }

    if (req->obj_id % sample_ratio_ != 0) {
      /* skip this object */
      return;
    }

    if (access_rtime_map_.find(req->obj_id) == access_rtime_map_.end()) {
      access_rtime_map_[req->obj_id] = vector<uint32_t>();
      access_vtime_map_[req->obj_id] = vector<uint32_t>();
    }
    access_rtime_map_[req->obj_id].push_back(req->clock_time - start_rtime_);
    access_vtime_map_[req->obj_id].push_back(n_seen_req_);
  }

  void dump(string &path_base) {
    string ofile_path = path_base + ".accessRtime";
    ofstream ofs(ofile_path, ios::out | ios::trunc);
    ofs << "# " << path_base << "\n";
    ofs << "# access pattern real time, each line stores all the real time of "
           "requests to an object\n";

    // sort the timestamp list by the first timestamp
    vector<vector<uint32_t> *> sorted_rtime_vec;
    for (auto &p : access_rtime_map_) {
      sorted_rtime_vec.push_back(&p.second);
    }
    sort(sorted_rtime_vec.begin(), sorted_rtime_vec.end(),
         [](vector<uint32_t> *p1, vector<uint32_t> *p2) {
           return (*p1).at(0) < (*p2).at(0);
         });

    for (const auto *p : sorted_rtime_vec) {
      for (const uint32_t rtime : *p) {
        ofs << rtime << ",";
      }
      ofs << "\n";
    }
    ofs << "\n" << endl;
    ofs.close();

    ofs << "# " << path_base << "\n";
    string ofile_path2 = path_base + ".accessVtime";
    ofstream ofs2(ofile_path2, ios::out | ios::trunc);
    ofs2 << "# access pattern virtual time, each line stores all the virtual "
           "time of requests to an object\n";
    vector<vector<uint32_t> *> sorted_vtime_vec;
    for (auto &p : access_vtime_map_) {
      sorted_vtime_vec.push_back(&(p.second));
    }

    sort(sorted_vtime_vec.begin(), sorted_vtime_vec.end(),
         [](vector<uint32_t> *p1, vector<uint32_t> *p2) -> bool {
           return (*p1).at(0) < (*p2).at(0);
         });

    for (const vector<uint32_t> *p : sorted_vtime_vec) {
      for (const uint32_t vtime : *p) {
        ofs2 << vtime << ",";
      }
      ofs2 << "\n";
    }

    ofs2.close();
  }

 private:
  int64_t n_obj_ = 0;
  int64_t n_total_req_ = 0;
  int64_t n_seen_req_ = 0;
  int sample_ratio_ = 1001;

  int64_t start_rtime_ = -1;
  unordered_map<obj_id_t, vector<uint32_t>> access_rtime_map_;
  unordered_map<obj_id_t, vector<uint32_t>> access_vtime_map_;
};
}  // namespace analysis
