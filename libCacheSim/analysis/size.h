
#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../include/libCacheSim/macro.h"
#include "../include/libCacheSim/reader.h"

using namespace std;

namespace analysis {
class SizeDistribution {
  /**
   * track a few things
   * 1. obj_size request count
   * 2. obj_size object  count
   * 3. obj_size request count over time (heatmap plot)
   * 4. obj_size object  count over time (heatmap plot)
   */
 public:
  SizeDistribution() = default;
  explicit SizeDistribution(string &output_path, int time_window)
      : time_window_(time_window) {
    obj_size_req_cnt_.reserve(1e6);
    obj_size_obj_cnt_.reserve(1e6);

    turn_on_stream_dump(output_path);
  };

  ~SizeDistribution() {
    ofs_stream_req.close();
    ofs_stream_obj.close();
  }

  inline void add_req(request_t *req) {
    if (unlikely(next_window_ts_ == -1)) {
      next_window_ts_ = (int64_t)req->clock_time + time_window_;
    }

    /* request count */
    obj_size_req_cnt_[req->obj_size] += 1;

    /* object count */
    if (req->compulsory_miss) {
      obj_size_obj_cnt_[req->obj_size] += 1;
    }

    if (time_window_ <= 0) return;

    /* window request/object count is stored using vector */
    // MAX(log_{LOG_BASE}{(req->obj_size / SIZE_BASE)} , 0);
    //      int pos = MAX((int) (log((double) req->obj_size / SIZE_BASE) /
    //      log(LOG_BASE)), 0);
    int pos = (int)MAX(log((double)req->obj_size) / log_log_base, 0);
    if (pos >= window_obj_size_req_cnt_.size()) {
      window_obj_size_req_cnt_.resize(pos + 8, 0);
      window_obj_size_obj_cnt_.resize(pos + 8, 0);
    }

    /* window request count */
    window_obj_size_req_cnt_[pos] += 1;

    /* window object count */
    //    if (window_seen_obj.count(req->obj_id) == 0) {
    //      window_obj_size_obj_cnt_[pos] += 1;
    //      window_seen_obj.insert(req->obj_id);
    //    }

    if (req->first_seen_in_window) {
      window_obj_size_obj_cnt_[pos] += 1;
    }

    while (req->clock_time >= next_window_ts_) {
      stream_dump();
      window_obj_size_req_cnt_ = vector<uint32_t>(20, 0);
      window_obj_size_obj_cnt_ = vector<uint32_t>(20, 0);
      next_window_ts_ += time_window_;
    }
  }

  void dump(string &path_base) {
    ofstream ofs(path_base + ".size", ios::out | ios::trunc);
    ofs << "# " << path_base << "\n";
    ofs << "# object_size: req_cnt\n";
    for (auto &p : obj_size_req_cnt_) {
      ofs << p.first << ":" << p.second << "\n";
    }

    ofs << "# object_size: obj_cnt\n";
    for (auto &p : obj_size_obj_cnt_) {
      ofs << p.first << ":" << p.second << "\n";
    }
    ofs.close();
  }

 private:
  /* request/object count of certain size, size->count */
  unordered_map<obj_size_t, uint32_t> obj_size_req_cnt_;
  unordered_map<obj_size_t, uint32_t> obj_size_obj_cnt_;

  /* used to plot size distribution heatmap */
  const double LOG_BASE = 1.5;
  const double log_log_base = log(LOG_BASE);
  const int time_window_ = 300;
  int64_t next_window_ts_ = -1;

  vector<uint32_t> window_obj_size_req_cnt_;
  vector<uint32_t> window_obj_size_obj_cnt_;

  ofstream ofs_stream_req;
  ofstream ofs_stream_obj;

  void turn_on_stream_dump(string &path_base) {
    ofs_stream_req.open(
        path_base + ".sizeWindow_w" + to_string(time_window_) + "_req",
        ios::out | ios::trunc);
    ofs_stream_req << "# " << path_base << "\n";
    ofs_stream_req << "# object_size: req_cnt (time window " << time_window_
                   << ", log_base " << LOG_BASE << ", size_base " << 1 << ")\n";

    ofs_stream_obj.open(
        path_base + ".sizeWindow_w" + to_string(time_window_) + "_obj",
        ios::out | ios::trunc);
    ofs_stream_obj << "# " << path_base << "\n";
    ofs_stream_obj << "# object_size: obj_cnt (time window " << time_window_
                   << ", log_base " << LOG_BASE << ", size_base " << 1 << ")\n";
  }

  void stream_dump() {
    for (const auto &p : window_obj_size_req_cnt_) {
      ofs_stream_req << p << ",";
    }
    ofs_stream_req << "\n";

    for (const auto &p : window_obj_size_obj_cnt_) {
      ofs_stream_obj << p << ",";
    }
    ofs_stream_obj << "\n";
  }
};

}  // namespace analysis