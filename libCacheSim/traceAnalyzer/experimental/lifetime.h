
#pragma once
/* plot the reuse distribution */

#include <unordered_map>
#include <vector>

#include "../../include/libCacheSim/reader.h"
#include "../utils/include/utils.h"

namespace traceAnalyzer {
class LifetimeDistribution {
 public:
  LifetimeDistribution() = default;

  ~LifetimeDistribution() = default;

  void add_req(request_t *req);

  void dump(string &path_base);

 private:
  int64_t n_req_ = 0;
  unordered_map<int64_t, pair<int64_t, int64_t>> first_last_req_clock_time_map_;
  unordered_map<int64_t, pair<int64_t, int64_t>>
      first_last_req_logical_time_map_;

  const int64_t object_create_clock_time_max_ = 86400;
};

}  // namespace traceAnalyzer
