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

#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "../include/libCacheSim/macro.h"
#include "../include/libCacheSim/request.h"
#include "struct.h"

// #define USE_REQ_METRIC 1

namespace traceAnalyzer {

class PopularityDecay {
 public:
  /* params */
  int warmup_rtime_;
  int time_window_;

  PopularityDecay(std::string &path_base, int time_window = 300,
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

  void turn_on_stream_dump(std::string &path_base);

  void add_req(const request_t *req);

  inline int time_to_window_idx(uint32_t rtime) { return rtime / time_window_; }

  void dump(std::string &path_base) { ; }

 private:
  int64_t next_window_ts_ = -1;
  /** how many of requests (objects) in current window are requesting objects
   * created in previous N windows for example, idx 0 stores the number of
   * requests (objects) in current window are for objects created in the first
   * window in the trace (after warmup) */
  std::vector<int32_t> n_req_per_window;
  std::vector<int32_t> n_obj_per_window;

  std::ofstream stream_dump_req_ofs;
  std::ofstream stream_dump_obj_ofs;
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
    stream_dump_obj_ofs << std::endl;
  }
};
}  // namespace traceAnalyzer