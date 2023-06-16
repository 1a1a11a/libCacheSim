#pragma once
/* plot the probability of access vs access age and create age,
 * and calculate the entropy of random variables X_ProbAtAge and X_createAge
 * which follow probability distribution P(R_ProbAtAge) and P(R_createAge)
 */

#include <unordered_map>
#include <vector>

#include "../../include/libCacheSim/reader.h"
#include "../struct.h"
#include "../utils/include/utils.h"

namespace traceAnalyzer {
using namespace std;
class ProbAtAge {
 public:
  explicit ProbAtAge(int time_window = 300, int warmup_rtime = 86400)
      : time_window_(time_window), warmup_rtime_(warmup_rtime){};

  ~ProbAtAge() = default;

  void add_req(request_t *req);

  void dump(string &path_base);

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

}  // namespace traceAnalyzer
