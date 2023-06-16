#include "createFutureReuseCCDF.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../struct.h"
#include "../utils/include/utils.h"

namespace traceAnalyzer {
using namespace std;

void CreateFutureReuseDistribution::add_req(request_t *req) {
  if (req->clock_time < warmup_time_) {
    warmup_obj_.insert(req->obj_id);
    return;
  }
  if (warmup_obj_.find(req->obj_id) != warmup_obj_.end()) {
    return;
  }

  assert(req->next_access_vtime != -2);
  auto it = create_info_map_.find(req->obj_id);

  if (it == create_info_map_.end()) {
    /* this is a new object */
    if (req->clock_time > warmup_time_ + creation_time_window_) {
      return;
    }
    create_info_map_[req->obj_id] = {(int32_t)req->clock_time, -1, 0};
    it = create_info_map_.find(req->obj_id);
  } else {
    //      assert(req->next_access_vtime != -1);
    // printf("%ld %ld %ld %ld\n",
    //      req->clock_time, req->obj_id, it->second.create_rtime,
    //      it->second.freq);
    create_info_map_[req->obj_id] = {
        it->second.create_rtime, (int32_t)req->clock_time, it->second.freq + 1};
  }

  int rtime_since_create = (int)req->clock_time - it->second.create_rtime;
  assert(rtime_since_create >= 0);

  int pos_rt = GET_POS_RT(rtime_since_create);
  assert(pos_rt < reuse_info_array_size_);

  if (req->next_access_vtime >= 0 && req->next_access_vtime != INT64_MAX) {
    reuse_info_[pos_rt].reuse_cnt += 1;
    reuse_info_[pos_rt].reuse_freq_sum += it->second.freq;
    int32_t access_age =
        it->second.last_read_rtime == -1
            ? 0
            : (int32_t)req->clock_time - it->second.last_read_rtime;
    reuse_info_[pos_rt].reuse_access_age_sum += access_age;
  } else {
    /* either no more requests, or the next request is write / delete */
    reuse_info_[pos_rt].stop_reuse_cnt += 1;
    reuse_info_[pos_rt].stop_reuse_freq_sum += it->second.freq;
    int32_t access_age =
        it->second.last_read_rtime == -1
            ? 0
            : (int32_t)req->clock_time - it->second.last_read_rtime;
    reuse_info_[pos_rt].stop_reuse_access_age_sum += access_age;
    // do not erase because we want create time
    // create_info_map_.erase(req->obj_id);
  }
}

void CreateFutureReuseDistribution::dump(string &path_base) {
  ofstream ofs(path_base + ".createFutureReuseCCDF", ios::out | ios::trunc);
  ofs << "# " << path_base << "\n";
  ofs << "# real time: reuse_cnt, stop_reuse_cnt, reuse_access_age_sum, "
         "stop_reuse_access_age_sum, "
      << "reuse_freq_sum, stop_reuse_freq_sum\n";
  for (int idx = 0; idx < this->reuse_info_array_size_; idx++) {
    int ts = GET_RTIME_FROM_POS(idx);
    if (ts == -1) continue;
    if (this->reuse_info_[idx].reuse_cnt +
            this->reuse_info_[idx].stop_reuse_cnt ==
        0)
      continue;
    ofs << ts << ":" << this->reuse_info_[idx].reuse_cnt << ","
        << this->reuse_info_[idx].stop_reuse_cnt << ","
        << this->reuse_info_[idx].reuse_access_age_sum << ","
        << this->reuse_info_[idx].stop_reuse_access_age_sum << ","
        << this->reuse_info_[idx].reuse_freq_sum << ","
        << this->reuse_info_[idx].stop_reuse_freq_sum << "\n";
  }
  // ofs << "-1: " << this->create_info_map_.size() << "\n";
}

};  // namespace traceAnalyzer
