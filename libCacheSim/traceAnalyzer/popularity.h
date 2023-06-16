#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <unordered_map>
#include <vector>

#include "../include/libCacheSim/logging.h"
#include "struct.h"
#include "utils/include/linReg.h"

namespace traceAnalyzer {
class PopularityUtils {
 public:
  PopularityUtils() = delete;
  ~PopularityUtils() = delete;
  static double slope(const std::vector<double> &x,
                      const std::vector<double> &y) {
    const auto n = x.size();
    const auto s_x = std::accumulate(x.begin(), x.end(), 0.0);
    const auto s_y = std::accumulate(y.begin(), y.end(), 0.0);
    const auto s_xx = std::inner_product(x.begin(), x.end(), x.begin(), 0.0);
    const auto s_xy = std::inner_product(x.begin(), x.end(), y.begin(), 0.0);
    const auto a = (n * s_xy - s_x * s_y) / (n * s_xx - s_x * s_x);
    return a;
  }
};

class Popularity {
 public:
  Popularity() { has_run = false; };
  ~Popularity() = default;

  explicit Popularity(obj_info_map_type &obj_map) { run(obj_map); };

  friend std::ostream &operator<<(std::ostream &os,
                                  const Popularity &popularity) {
    if (popularity.freq_vec_.empty()) {
      ERROR("popularity has not been computed\n");
      return os;
    }

    if (popularity.fit_fail_reason_.size() > 0)
      os << popularity.fit_fail_reason_ << "\n";
    else
      os << std::setprecision(4)
         << "popularity: Zipf linear fitting slope=" << popularity.slope_
         << ", intercept=" << popularity.intercept_ << ", R2=" << popularity.r2_
         << "\n";

    return os;
  }

  std::vector<uint32_t> &get_sorted_freq() { return freq_vec_; }

  void dump(std::string &path_base);

  std::string fit_fail_reason_ = "";

 private:
  void run(obj_info_map_type &obj_map);

  std::vector<uint32_t> freq_vec_{};
  double slope_ = -1, intercept_ = -1, r2_ = -1;
  bool has_run = false;
};
}  // namespace traceAnalyzer