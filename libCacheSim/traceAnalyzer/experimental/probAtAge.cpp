#include "probAtAge.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../include/libCacheSim/reader.h"
#include "../struct.h"
#include "../utils/include/utils.h"

namespace traceAnalyzer {
using namespace std;
void ProbAtAge::add_req(request_t *req) {
  if (req->clock_time < warmup_rtime_ || req->create_rtime < warmup_rtime_) {
    return;
  }

  int pos_access = (int)(req->rtime_since_last_access / time_window_);
  int pos_create = (int)((req->clock_time - req->create_rtime) / time_window_);

  auto p = pair<int32_t, int32_t>(pos_access, pos_create);
  ac_age_req_cnt_[p] += 1;
}

void ProbAtAge::dump(string &path_base) {
  ofstream ofs2(path_base + ".probAtAge_w" + std::to_string(time_window_),
                ios::out | ios::trunc);
  ofs2 << "# " << path_base << "\n";
  ofs2 << "# reuse access age, create age: req_cnt\n";
  for (auto &p : ac_age_req_cnt_) {
    ofs2 << p.first.first * time_window_ << "," << p.first.second * time_window_
         << ":" << p.second << "\n";
  }
}

};  // namespace traceAnalyzer
