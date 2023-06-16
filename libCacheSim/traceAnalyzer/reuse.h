
#pragma once
/* plot the reuse distribution */

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

#include "../include/libCacheSim/reader.h"
#include "utils/include/utils.h"
#include "struct.h"

using namespace std;

namespace analysis {
class ReuseDistribution {
 public:
  explicit ReuseDistribution(string output_path, int time_window = 300,
                             int rtime_granularity = 5,
                             int vtime_granularity = 1000)
      : time_window_(time_window),
        rtime_granularity_(rtime_granularity),
        vtime_granularity_(vtime_granularity) {
    turn_on_stream_dump(output_path);
  };

  ~ReuseDistribution() {
    stream_dump_rt_ofs.close();
    stream_dump_vt_ofs.close();
  }

  inline void add_req(request_t *req) {
    if (unlikely(next_window_ts_ == -1)) {
      next_window_ts_ = (int64_t)req->clock_time + time_window_;
    }

    if (req->rtime_since_last_access < 0) {
      reuse_rtime_req_cnt_[-1] += 1;
      reuse_vtime_req_cnt_[-1] += 1;

      return;
    }

    int pos_rt = (int)(req->rtime_since_last_access / rtime_granularity_);
    int pos_vt =
        (int)(log(double(req->vtime_since_last_access)) / log_log_base_);

    reuse_rtime_req_cnt_[pos_rt] += 1;
    reuse_vtime_req_cnt_[pos_vt] += 1;

    //    switch (req->op) {
    //      case OP_GET:
    //      case OP_GETS:
    //        reuse_rtime_req_cnt_read_[pos_rt] += 1;
    //        break;
    //      case OP_SET:
    //      case OP_ADD:
    //      case OP_REPLACE:
    //      case OP_CAS:
    //        reuse_rtime_req_cnt_write_[pos_rt] += 1;
    //        break;
    //      case OP_DELETE:
    //        reuse_rtime_req_cnt_delete_[pos_rt] += 1;
    //        break;
    //      default:
    //        break;
    //    }

    if (time_window_ <= 0) return;

    utils::vector_incr(window_reuse_rtime_req_cnt_, pos_rt, (uint32_t)1);
    utils::vector_incr(window_reuse_vtime_req_cnt_, pos_vt, (uint32_t)1);

    while (req->clock_time >= next_window_ts_) {
      stream_dump_window_reuse_distribution();
      window_reuse_rtime_req_cnt_.clear();
      window_reuse_vtime_req_cnt_.clear();
      next_window_ts_ += time_window_;
    }
  }

  void dump(string &path_base) {
    ofstream ofs(path_base + ".reuse", ios::out | ios::trunc);
    ofs << "# " << path_base << "\n";
    ofs << "# reuse real time: freq (time granularity " << rtime_granularity_
        << ")\n";
    for (auto &p : reuse_rtime_req_cnt_) {
      ofs << p.first << ":" << p.second << "\n";
    }

    ofs << "# reuse virtual time: freq (log base " << log_base_ << ")\n";
    for (auto &p : reuse_vtime_req_cnt_) {
      ofs << p.first << ":" << p.second << "\n";
    }
    ofs.close();

    //    if (std::accumulate(reuse_rtime_req_cnt_read_.begin(),
    //    reuse_rtime_req_cnt_read_.end(), 0) == 0)
    //      return;
    //
    //    ofs.open(path_base + ".reuse_op", ios::out | ios::trunc);
    //    ofs << "# " << path_base << "\n";
    //    ofs << "# reuse real time: freq (time granularity " <<
    //    rtime_granularity_ << ")\n";
  }

 private:
  /* request count for reuse rtime/vtime */
  unordered_map<int32_t, uint32_t> reuse_rtime_req_cnt_;
  unordered_map<int32_t, uint32_t> reuse_vtime_req_cnt_;

  unordered_map<int32_t, uint32_t> reuse_rtime_req_cnt_read_;
  unordered_map<int32_t, uint32_t> reuse_rtime_req_cnt_write_;
  unordered_map<int32_t, uint32_t> reuse_rtime_req_cnt_delete_;

  /* used to plot reuse distribution heatmap */
  const double log_base_ = 1.5;
  const double log_log_base_ = log(log_base_);
  const int rtime_granularity_;
  const int vtime_granularity_;
  const int time_window_;
  int64_t next_window_ts_ = -1;

  vector<uint32_t> window_reuse_rtime_req_cnt_;
  vector<uint32_t> window_reuse_vtime_req_cnt_;

  ofstream stream_dump_rt_ofs;
  ofstream stream_dump_vt_ofs;

  void turn_on_stream_dump(string &path_base) {
    stream_dump_rt_ofs.open(
        path_base + ".reuseWindow_w" + to_string(time_window_) + "_rt",
        ios::out | ios::trunc);
    stream_dump_rt_ofs << "# " << path_base << "\n";
    stream_dump_rt_ofs
        << "# reuse real time distribution per window (time granularity ";
    stream_dump_rt_ofs << rtime_granularity_ << ", time window " << time_window_
                       << ")\n";

    stream_dump_vt_ofs.open(
        path_base + ".reuseWindow_w" + to_string(time_window_) + "_vt",
        ios::out | ios::trunc);
    stream_dump_vt_ofs << "# " << path_base << "\n";
    stream_dump_vt_ofs
        << "# reuse virtual time distribution per window (log base "
        << log_base_;
    stream_dump_vt_ofs << ", time window " << time_window_ << ")\n";
  }

  void stream_dump_window_reuse_distribution() {
    for (const auto &p : window_reuse_rtime_req_cnt_) {
      stream_dump_rt_ofs << p << ",";
    }
    stream_dump_rt_ofs << "\n";

    for (const auto &p : window_reuse_vtime_req_cnt_) {
      stream_dump_vt_ofs << p << ",";
    }
    stream_dump_vt_ofs << "\n";
  }
};

}  // namespace analysis
