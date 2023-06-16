#pragma once

#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../include/libCacheSim/request.h"

namespace traceAnalyzer {

class TtlStat {
 public:
  TtlStat() = default;
  ~TtlStat() = default;

  void add_req(request_t* req);

  friend std::ostream& operator<<(std::ostream& os, const TtlStat& ttl) {
    std::stringstream stat_ss;

    std::cout << "TTL: " << ttl.ttl_cnt_.size() << " different TTLs, ";
    uint64_t n_req = std::accumulate(
        std::begin(ttl.ttl_cnt_), std::end(ttl.ttl_cnt_), 0ULL,
        [](uint64_t value,
           const std::unordered_map<int32_t, uint32_t>::value_type& p) {
          return value + p.second;
        });

    if (ttl.ttl_cnt_.size() > 1) {
      stat_ss << "TTL: " << ttl.ttl_cnt_.size() << " different TTLs, ";
      for (auto it : ttl.ttl_cnt_) {
        if (it.second > (size_t)((double)n_req * 0.01)) {
          stat_ss << it.first << ":" << it.second << "("
                  << (double)it.second / (double)n_req << "), ";
        }
      }
      stat_ss << "\n";
    }
    os << stat_ss.str();

    return os;
  }

  void dump(const std::string& filename);

 private:
  /* the number of requests have ttl value */
  std::unordered_map<int32_t, uint32_t> ttl_cnt_{};
  bool too_many_ttl_ = false;
};
}  // namespace traceAnalyzer