
#pragma once
/* calculate the reuse CCDF at certain create age, each point (x, y) in the
 * distribution measures the probability of having future reuse y at age x */

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

#include "../include/libCacheSim/reader.h"
#include "struct.h"
#include "utils/include/utils.h"

namespace analysis {
using namespace std;

class CreateFutureReuseDistribution {
 public:
  explicit CreateFutureReuseDistribution(int warmup_time = 7200)
      : warmup_time_(warmup_time) {
    utils::my_assert(
        reuse_info_array_size_ ==
            max_reuse_rtime_ / rtime_granularity_ + fine_rtime_upbound_,
        "reuse_info_array_size_ problem");
    reuse_info_ = new create_reuse_info_t[reuse_info_array_size_];
    memset(reuse_info_, 0,
           sizeof(create_reuse_info_t) * reuse_info_array_size_);
  }

  ~CreateFutureReuseDistribution() { delete[] reuse_info_; }

  inline int GET_POS_RT(int t) {
    if (t > fine_rtime_upbound_)
      return (int)(t / rtime_granularity_) + fine_rtime_upbound_;
    else
      return t;
  }

  inline int GET_RTIME_FROM_POS(int p) {
    if (p > fine_rtime_upbound_) {
      /* this is not a valid pos, because there is a gap between fine_rtime and
       * coarse_rtime */
      if (p < fine_rtime_upbound_ + fine_rtime_upbound_ / rtime_granularity_)
        return -1;
      else
        return (p - fine_rtime_upbound_) * rtime_granularity_;
    } else {
      return p;
    }
  }

  inline void add_req(request_t *req) {
    if (req->clock_time < warmup_time_) {
      warmup_obj_.insert(req->obj_id);
      return;
    }
    if (warmup_obj_.find(req->obj_id) != warmup_obj_.end()) {
      return;
    }

    utils::my_assert(req->next_access_vtime != -2,
                     "trace may not be oracleReuse trace\n");
    auto it = create_info_map_.find(req->obj_id);

    if (it == create_info_map_.end()) {
      /* this is a new object */
      if (req->clock_time > warmup_time_ + creation_time_window_) {
        return;
      }
      create_info_map_[req->obj_id] = {(int32_t)req->clock_time, -1, 0};
      it = create_info_map_.find(req->obj_id);
    } else {
      //      utils::my_assert(req->next_access_vtime != -1, "find requests with
      //      next_access_vtime -1\n"); printf("%ld %ld %ld %ld\n",
      //      req->clock_time, req->obj_id, it->second.create_rtime,
      //      it->second.freq);
      create_info_map_[req->obj_id] = {it->second.create_rtime,
                                       (int32_t)req->clock_time,
                                       it->second.freq + 1};
    }

    int rtime_since_create = (int)req->clock_time - it->second.create_rtime;
    utils::my_assert(
        rtime_since_create >= 0,
        "rtime since write " + to_string(rtime_since_create) + " < 0\n");

    int pos_rt = GET_POS_RT(rtime_since_create);
    utils::my_assert(pos_rt < reuse_info_array_size_,
                     "pos larger than reuse_info_array_size_\n");

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

  void dump(string &path_base) {
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

 private:
  struct create_info {
    int32_t create_rtime;
    int32_t last_read_rtime;
    int32_t freq;
  } __attribute__((packed));

  /* obj -> (write_time, read_time), read time is used to check it has been read
   */
  //  unordered_map<obj_id_t, struct create_info> create_info_map_;
  robin_hood::unordered_flat_map<obj_id_t, struct create_info> create_info_map_;

  /* future reuse (whether it has future request) in the struct */
  typedef struct {
    uint32_t reuse_cnt;
    uint32_t stop_reuse_cnt;
    uint64_t reuse_access_age_sum;
    uint64_t reuse_freq_sum;
    uint64_t stop_reuse_access_age_sum;
    uint64_t stop_reuse_freq_sum;
  } create_reuse_info_t;
  /* the request cnt that get a reuse or stop reuse at create age */
  create_reuse_info_t *reuse_info_;

  const int warmup_time_ = 86400;
  unordered_set<obj_id_t> warmup_obj_;

  /* we only consider objects created between warmup_time_ +
   * creation_time_window_ to avoid the impact of trace ending */
  const int creation_time_window_ = 86400;

  /* rtime smaller than fine_rtime_upbound_ will not be divide by
   * rtime_granularity_ */
  const int fine_rtime_upbound_ = 3600;
  const int rtime_granularity_ = 5;

  const int max_reuse_rtime_ = 86400 * 80;
  const int reuse_info_array_size_ =
      max_reuse_rtime_ / rtime_granularity_ + fine_rtime_upbound_;
};

}  // namespace analysis
