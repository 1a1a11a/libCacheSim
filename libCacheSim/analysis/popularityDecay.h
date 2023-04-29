#pragma once

/**
 * calculate popularity fade as a heatmap,
 * we first warm up a cache using warmup_rtime,
 * then we try to calculate how the popularity (frequency) of new objects change
 * over time.
 *
 * we call frequency sum (F_0) of all the *new* objects in a time_window as the
 * base_popularity, then for the next, next next, ... time_window, we calculate
 * how many requests are for these objects and the frequency sum is F_1, F_2 ...
 * we calculate the relative frequency sum as F_i / F_0, which is how the
 * popularity of a group of new object changes over time, so the relative
 * popularity of the first window is 1.0, next is F_1/F_0
 *
 * We do this for each time window, then we will have a lower triangular matrix
 * but with the minor diagonal illustration, for x at row (i, j), it means n_obj
 * or n_req of objects/requests created at time window j are requested at time
 * window i-1 the first line is 0, and the last element of each row is 0
 * 0
 * x 0
 * x x 0
 * x x x 0
 * x x x x 0
 * x x x x x 0
 * x x x x x x 0
 *
 * each row i is the objects started at the time window i
 *
 *
 */

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>
#include <vector>

#include "../include/libCacheSim/macro.h"
#include "struct.h"

using namespace std;

// #define USE_REQ_METRIC 1

namespace analysis {

class PopularityDecay {
 public:
  /* params */
  int warmup_rtime_;
  int time_window_;

  PopularityDecay(string &path_base, int time_window = 300,
                  int warmup_rtime = 7200)
      : time_window_(time_window), warmup_rtime_(warmup_rtime) {
    n_req_per_window.resize(1, 0);
    n_obj_per_window.resize(1, 0);
    idx_shift = (int)((double)warmup_rtime / time_window);
    turn_on_stream_dump(path_base);
  };

  ~PopularityDecay() {
#ifdef USE_REQ_METRIC
    stream_dump_req_ofs.close();
#endif
    stream_dump_obj_ofs.close();
  }

  void turn_on_stream_dump(string &path_base) {
#ifdef USE_REQ_METRIC
    stream_dump_req_ofs.open(
        path_base + ".popularityDecay_w" + to_string(time_window_) + "_req",
        ios::out | ios::trunc);
    stream_dump_req_ofs << "# " << path_base << "\n";
    stream_dump_req_ofs
        << "# req_cnt for new object in prev N windows (time window "
        << time_window_ << ")\n";
#endif

    stream_dump_obj_ofs.open(
        path_base + ".popularityDecay_w" + to_string(time_window_) + "_obj",
        ios::out | ios::trunc);
    stream_dump_obj_ofs << "# " << path_base << "\n";
    stream_dump_obj_ofs
        << "# obj_cnt for new object in prev N windows (time window "
        << time_window_ << ")\n";
  }

  void add_req(const request_t *req) {
    if (unlikely(next_window_ts_ == -1)) {
      next_window_ts_ = (int64_t)req->clock_time + time_window_;
    }

    /* this assumes req real time starts from 0 */
    if (req->clock_time < warmup_rtime_) {
      while (req->clock_time >= next_window_ts_) {
        next_window_ts_ += time_window_;
      }
      return;
    }

    int create_time_window_idx = time_to_window_idx(req->create_rtime);
    if (create_time_window_idx < idx_shift) {
      // the object is created during warm up
      return;
    }

    while (req->clock_time >= next_window_ts_) {
      if (next_window_ts_ < warmup_rtime_) {
        /* if the timestamp jumped (no requests in some time windows) */
        next_window_ts_ = warmup_rtime_;
        continue;
      }
      /* because req->clock_time < warmup_rtime_ not <= on line 97,
       * the first line of output will be 0, and the last of each line is 0 */
      stream_dump();
      n_req_per_window.assign(n_req_per_window.size() + 1, 0);
      n_obj_per_window.assign(n_obj_per_window.size() + 1, 0);
      next_window_ts_ += time_window_;
    }

    assert(create_time_window_idx - idx_shift < n_req_per_window.size());

    n_req_per_window.at(create_time_window_idx - idx_shift) += 1;
    if (req->first_seen_in_window) {
      n_obj_per_window.at(create_time_window_idx - idx_shift) += 1;
    }
  }

  inline int time_to_window_idx(uint32_t rtime) { return rtime / time_window_; }

  void dump(string &path_base) { ; }

 private:
  int64_t next_window_ts_ = -1;
  /** how many of requests (objects) in current window are requesting objects
   * created in previous N windows for example, idx 0 stores the number of
   * requests (objects) in current window are for objects created in the first
   * window in the trace (after warmup) */
  vector<int32_t> n_req_per_window;
  vector<int32_t> n_obj_per_window;

  ofstream stream_dump_req_ofs;
  ofstream stream_dump_obj_ofs;
  int idx_shift = -1;

  void stream_dump() {
#ifdef USE_REQ_METRIC
    for (auto n_req : n_req_per_window) {
      stream_dump_req_ofs << n_req << ",";
    }
    stream_dump_req_ofs << endl;
#endif

    for (auto n_obj : n_obj_per_window) {
      stream_dump_obj_ofs << n_obj << ",";
    }
    stream_dump_obj_ofs << endl;
  }
};
}  // namespace analysis