
#include "popularity.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <numeric>
#include <unordered_map>
#include <vector>

#include "struct.h"
#include "utils/include/linReg.h"

namespace traceAnalyzer {
using namespace std;

void Popularity::dump(string &path_base) {
  if (freq_vec_.empty()) {
    assert(!has_run);
    ERROR("popularity has not been computed\n");
    return;
  }

  string ofile_path = path_base + ".popularity";
  ofstream ofs(ofile_path, ios::out | ios::trunc);
  ofs << "# " << path_base << "\n";
  ofs << "# freq (sorted):cnt - for Zipf plot\n";

  /* convert sorted freq list to freq:cnt list sorted by freq (to save some
   * space) */
  uint32_t last_freq = freq_vec_[0];
  uint32_t freq_cnt = 0;
  for (auto &cnt : freq_vec_) {
    if (cnt == last_freq) {
      freq_cnt += 1;
    } else {
      ofs << last_freq << ":" << freq_cnt << "\n";
      freq_cnt = 1;
      last_freq = cnt;
    }
  }
  ofs << last_freq << ":" << freq_cnt << "\n";
  ofs.close();
}

void Popularity::run(obj_info_map_type &obj_map) {
  /* freq_vec_ is a sorted vec of obj frequency */
  freq_vec_.reserve(obj_map.size());
  for (const auto &p : obj_map) {
    freq_vec_.push_back(p.second.freq);
  }
  sort(freq_vec_.begin(), freq_vec_.end(), greater<>());

  if (obj_map.size() < 200) {
    fit_fail_reason_ = "popularity: too few objects (" +
                       to_string(obj_map.size()) +
                       "), skip the popularity computation";
    WARN("%s\n", fit_fail_reason_.c_str());
    return;
  }

  if (freq_vec_[0] < 200) {
    fit_fail_reason_ = "popularity: the most popular object has " +
                       to_string(freq_vec_[0]) + " requests ";
    WARN("%s\n", fit_fail_reason_.c_str());
  }

  /* calculate Zipf alpha using linear regression: log(freq) = -alpha*log(rank) + c */
  const size_t n = freq_vec_.size();
  vector<double> log_freq(n);
  vector<double> log_rank(n);

  for (size_t i = 0; i < n; i++) {
    log_freq[i] = log(static_cast<double>(freq_vec_[i]));
    log_rank[i] = log(static_cast<double>(i + 1));
  }

  double reg_slope, reg_intercept, r;
  int err = linreg(static_cast<int>(n), log_rank.data(), log_freq.data(),
                  &reg_slope, &reg_intercept, &r);
  if (err != 0) {
    fit_fail_reason_ = "popularity: singular regression matrix (e.g. uniform)";
    WARN("%s\n", fit_fail_reason_.c_str());
    return;
  }
  /* Zipf: log(freq) = -alpha*log(rank) + c, so reg_slope = -alpha */
  slope_ = -reg_slope;
  intercept_ = reg_intercept;
  r2_ = (std::isfinite(r) ? r * r : 0.0);

  has_run = true;
}

};  // namespace traceAnalyzer
