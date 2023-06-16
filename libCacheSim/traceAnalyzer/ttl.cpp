
#include "ttl.h"

#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "../include/libCacheSim/request.h"

namespace traceAnalyzer {
using namespace std;

void TtlStat::add_req(request_t *req) {
  printf("req->ttl: %d\n", req->ttl);
  
  if (req->ttl > 0) {
    auto it = ttl_cnt_.find(req->ttl);
    if (it == ttl_cnt_.end()) {
      ttl_cnt_[req->ttl] = 1;
      if ((!too_many_ttl_) && ttl_cnt_.size() > 1000000) {
        too_many_ttl_ = true;
        WARN("there are too many TTLs (%zu) in the trace\n", ttl_cnt_.size());
      }
    } else {
      it->second += 1;
    }
  }
}

void TtlStat::dump(const string &path_base) {
  ofstream ofs(path_base + ".ttl", ios::out | ios::trunc);
  ofs << "# " << path_base << "\n";
  if (too_many_ttl_) {
    ofs << "# there are too many TTLs in the trace\n";
    return;
  } else {
    ofs << "# TTL: req_cnt\n";
    for (auto &p : ttl_cnt_) {
      ofs << p.first << ":" << p.second << "\n";
    }
  }
  ofs.close();
}
};  // namespace traceAnalyzer
