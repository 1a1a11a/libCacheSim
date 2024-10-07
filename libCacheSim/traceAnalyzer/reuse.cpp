
#include "reuse.h"

#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../include/libCacheSim/macro.h"

namespace traceAnalyzer {
using namespace std;

void ReuseDistribution::add_req(request_t *req) {
  if (unlikely(next_window_ts_ == -1)) {
    next_window_ts_ = (int64_t)req->clock_time + time_window_;
  }

  if (req->rtime_since_last_access < 0) {
    // compulsory miss
    reuse_rtime_req_cnt_[-1] += 1;
    reuse_vtime_req_cnt_[-1] += 1;

    return;
  }

  int pos_rt = (int)(req->rtime_since_last_access / rtime_granularity_);
  int pos_vt = (int)(log(double(req->vtime_since_last_access)) / log_log_base_);

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

void ReuseDistribution::dump(string &path_base) {
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

void ReuseDistribution::turn_on_stream_dump(string &path_base) {
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
      << "# reuse virtual time distribution per window (log base " << log_base_;
  stream_dump_vt_ofs << ", time window " << time_window_ << ")\n";
}

void ReuseDistribution::stream_dump_window_reuse_distribution() {
  for (const auto &p : window_reuse_rtime_req_cnt_) {
    stream_dump_rt_ofs << p << ",";
  }
  stream_dump_rt_ofs << "\n";

  for (const auto &p : window_reuse_vtime_req_cnt_) {
    stream_dump_vt_ofs << p << ",";
  }
  stream_dump_vt_ofs << "\n";
}
};  // namespace traceAnalyzer
