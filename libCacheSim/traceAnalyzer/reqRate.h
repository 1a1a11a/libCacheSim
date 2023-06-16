#pragma once

#include <algorithm>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <vector>

#include "../include/libCacheSim/macro.h"
#include "../include/libCacheSim/request.h"

namespace traceAnalyzer {

class ReqRate {
 public:
  ReqRate() = default;
  explicit ReqRate(int time_window) : time_window_(time_window){};
  ~ReqRate() = default;

  void add_req(request_t *req);

  void dump(const std::string &path_base);

  friend std::ostream &operator<<(std::ostream &os, const ReqRate &rr) {
    if (rr.req_rate_.size() < 10) {
      WARN("request rate not enough window (%zu window)\n",
           rr.req_rate_.size());
      if (rr.req_rate_.empty()) return os;
    }
    double max =
        (double)*std::max_element(rr.req_rate_.cbegin(), rr.req_rate_.cend()) /
        rr.time_window_;
    double min =
        (double)*std::min_element(rr.req_rate_.cbegin(), rr.req_rate_.cend()) /
        rr.time_window_;
    os << std::fixed << "request rate min " << min << " req/s, max " << max
       << " req/s, window " << rr.time_window_ << "s\n";

    max =
        (double)*std::max_element(rr.obj_rate_.cbegin(), rr.obj_rate_.cend()) /
        rr.time_window_;
    min =
        (double)*std::min_element(rr.obj_rate_.cbegin(), rr.obj_rate_.cend()) /
        rr.time_window_;
    os << std::fixed << "object rate min " << min << " obj/s, max " << max
       << " obj/s, window " << rr.time_window_ << "s\n";

    return os;
  }

 private:
  const int time_window_ = 60;
  /* objects have been seen in the time window */
  //  unordered_set<obj_id_t> window_seen_obj_{};
  int64_t next_window_ts_ = -1;
  /* how many requests in the window */
  uint32_t window_n_req_ = 0;
  /* how many bytes in the window */
  uint64_t window_n_byte_ = 0;
  /* how many objects in the window */
  uint32_t window_n_obj_ = 0;
  /* how many objects requested in the window are first-time in the trace */
  uint32_t window_compulsory_miss_obj_ = 0;
  std::vector<uint32_t> req_rate_{};
  std::vector<uint64_t> byte_rate_{}; /* bytes/sec */
  std::vector<uint32_t> obj_rate_{};
  /* used to calculate cold miss ratio over time */
  std::vector<uint32_t> first_seen_obj_rate_{};
};
}  // namespace traceAnalyzer