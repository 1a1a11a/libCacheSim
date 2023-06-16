#pragma once
/* plot the probability of access vs access age and create age,
 * and calculate the entropy of random variables X_ProbAtAge and X_createAge
 * which follow probability distribution P(R_ProbAtAge) and P(R_createAge)
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../include/libCacheSim/reader.h"
#include "struct.h"
#include "utils/include/utils.h"

namespace analysis {
using namespace std;
class ProbAtAge {
 public:
  explicit ProbAtAge(int time_window = 300, int warmup_rtime = 86400)
      : time_window_(time_window), warmup_rtime_(warmup_rtime){};

  ~ProbAtAge() = default;

  inline void add_req(request_t *req) {
    if (req->clock_time < warmup_rtime_ || req->create_rtime < warmup_rtime_) {
      return;
    }

    int pos_access = (int)(req->rtime_since_last_access / time_window_);
    int pos_create = (int)((req->clock_time - req->create_rtime) / time_window_);

    auto p = pair<int32_t, int32_t>(pos_access, pos_create);
    ac_age_req_cnt_[p] += 1;
  }

  void dump(string &path_base) {
    ofstream ofs2(path_base + ".probAtAge_w" + std::to_string(time_window_),
                  ios::out | ios::trunc);
    ofs2 << "# " << path_base << "\n";
    ofs2 << "# reuse access age, create age: req_cnt\n";
    for (auto &p : ac_age_req_cnt_) {
      ofs2 << p.first.first * time_window_ << ","
           << p.first.second * time_window_ << ":" << p.second << "\n";
    }
  }

 private:
  struct pair_hash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2> &p) const {
      auto h1 = std::hash<T1>{}(p.first);
      auto h2 = std::hash<T2>{}(p.second);
      return h1 ^ h2;
    }
  };

  /* request count for reuse rtime */
  unordered_map<pair<int32_t, int32_t>, uint32_t, pair_hash> ac_age_req_cnt_;

  const int time_window_;
  const int warmup_rtime_;
};

}  // namespace analysis
