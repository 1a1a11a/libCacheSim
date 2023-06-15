

#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "../include/libCacheSim/request.h"

using namespace std;

namespace analysis {

class TtlStat {
 public:
  TtlStat() = default;
  ~TtlStat() = default;

  inline void add_req(request_t* req) {
    if (req->ttl > 0) {
      auto it2 = ttl_cnt_.find(req->ttl);
      if (it2 == ttl_cnt_.end()) {
        ttl_cnt_[req->ttl] = 1;
        if ((!too_many_ttl_) && ttl_cnt_.size() % 100000 == 0) {
          too_many_ttl_ = true;
          WARN("there are too many TTLs (%zu) in the trace\n", ttl_cnt_.size());
        }
      } else {
        it2->second += 1;
      }
    }
  }

  friend ostream& operator<<(ostream& os, const TtlStat& ttl) {
    stringstream stat_ss;
    uint64_t n_req =
        accumulate(begin(ttl.ttl_cnt_), end(ttl.ttl_cnt_), 0ULL,
                   [](uint64_t value,
                      const unordered_map<uint32_t, uint64_t>::value_type& p) {
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

 private:
  unordered_map<uint32_t, uint64_t>
      ttl_cnt_{}; /* the number of requests have ttl */
  bool too_many_ttl_ = false;
};
}  // namespace analysis