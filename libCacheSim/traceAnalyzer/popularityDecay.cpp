
#include "popularityDecay.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace traceAnalyzer {
using namespace std;

void PopularityDecay::turn_on_stream_dump(string &path_base) {
#ifdef USE_REQ_METRIC
  stream_dump_req_ofs.open(
      path_base + ".popularityDecay_w" + to_string(time_window_) + "_req",
      ios::out | ios::trunc);
  stream_dump_req_ofs << "# " << path_base << "\n";
  stream_dump_req_ofs
      << "# req_cnt for new object in prev N windows (time window "
      << time_window_ << ")\n";
#endif

  stream_dump_obj_ofs.open(
      path_base + ".popularityDecay_w" + to_string(time_window_) + "_obj",
      ios::out | ios::trunc);
  stream_dump_obj_ofs << "# " << path_base << "\n";
  stream_dump_obj_ofs
      << "# obj_cnt for new object in prev N windows (time window "
      << time_window_ << ")\n";
}

void PopularityDecay::add_req(const request_t *req) {
  if (unlikely(next_window_ts_ == -1)) {
    next_window_ts_ = (int64_t)req->clock_time + time_window_;
  }

  /* this assumes req real time starts from 0 */
  if (req->clock_time < warmup_rtime_) {
    while (req->clock_time >= next_window_ts_) {
      next_window_ts_ += time_window_;
    }
    return;
  }

  int create_time_window_idx = time_to_window_idx(req->create_rtime);
  if (create_time_window_idx < idx_shift) {
    // the object is created during warm up
    return;
  }

  while (req->clock_time >= next_window_ts_) {
    if (next_window_ts_ < warmup_rtime_) {
      /* if the timestamp jumped (no requests in some time windows) */
      next_window_ts_ = warmup_rtime_;
      continue;
    }
    /* because req->clock_time < warmup_rtime_ not <= on line 97,
     * the first line of output will be 0, and the last of each line is 0 */
    stream_dump();
    n_req_per_window.assign(n_req_per_window.size() + 1, 0);
    n_obj_per_window.assign(n_obj_per_window.size() + 1, 0);
    next_window_ts_ += time_window_;
  }

  assert(create_time_window_idx - idx_shift < n_req_per_window.size());

  n_req_per_window.at(create_time_window_idx - idx_shift) += 1;
  if (req->first_seen_in_window) {
    n_obj_per_window.at(create_time_window_idx - idx_shift) += 1;
  }
}

};  // namespace traceAnalyzer
