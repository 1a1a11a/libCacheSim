
#include "size.h"

#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <string>

namespace traceAnalyzer {
using namespace std;

void SizeDistribution::add_req(request_t *req) {
  if (unlikely(next_window_ts_ == -1)) {
    next_window_ts_ = (int64_t)req->clock_time + time_window_;
  }

  /* request count */
  obj_size_req_cnt_[req->obj_size] += 1;

  /* object count */
  if (req->compulsory_miss) {
    obj_size_obj_cnt_[req->obj_size] += 1;
  }

  if (time_window_ <= 0) return;

  /* window request/object count is stored using vector */
  // MAX(log_{LOG_BASE}{(req->obj_size / SIZE_BASE)} , 0);
  //      int pos = MAX((int) (log((double) req->obj_size / SIZE_BASE) /
  //      log(LOG_BASE)), 0);
  int pos = (int)MAX(log((double)req->obj_size) / log_log_base, 0);
  if (pos >= window_obj_size_req_cnt_.size()) {
    window_obj_size_req_cnt_.resize(pos + 8, 0);
    window_obj_size_obj_cnt_.resize(pos + 8, 0);
  }

  /* window request count */
  window_obj_size_req_cnt_[pos] += 1;

  /* window object count */
  //    if (window_seen_obj.count(req->obj_id) == 0) {
  //      window_obj_size_obj_cnt_[pos] += 1;
  //      window_seen_obj.insert(req->obj_id);
  //    }

  if (req->first_seen_in_window) {
    window_obj_size_obj_cnt_[pos] += 1;
  }

  while (req->clock_time >= next_window_ts_) {
    stream_dump();
    window_obj_size_req_cnt_ = vector<uint32_t>(20, 0);
    window_obj_size_obj_cnt_ = vector<uint32_t>(20, 0);
    next_window_ts_ += time_window_;
  }
}

void SizeDistribution::dump(string &path_base) {
  ofstream ofs(path_base + ".size", ios::out | ios::trunc);
  ofs << "# " << path_base << "\n";
  ofs << "# object_size: req_cnt\n";
  for (auto &p : obj_size_req_cnt_) {
    ofs << p.first << ":" << p.second << "\n";
  }

  ofs << "# object_size: obj_cnt\n";
  for (auto &p : obj_size_obj_cnt_) {
    ofs << p.first << ":" << p.second << "\n";
  }
  ofs.close();
}

void SizeDistribution::turn_on_stream_dump(string &path_base) {
  ofs_stream_req.open(
      path_base + ".sizeWindow_w" + to_string(time_window_) + "_req",
      ios::out | ios::trunc);
  ofs_stream_req << "# " << path_base << "\n";
  ofs_stream_req << "# object_size: req_cnt (time window " << time_window_
                 << ", log_base " << LOG_BASE << ", size_base " << 1 << ")\n";

  ofs_stream_obj.open(
      path_base + ".sizeWindow_w" + to_string(time_window_) + "_obj",
      ios::out | ios::trunc);
  ofs_stream_obj << "# " << path_base << "\n";
  ofs_stream_obj << "# object_size: obj_cnt (time window " << time_window_
                 << ", log_base " << LOG_BASE << ", size_base " << 1 << ")\n";
}

void SizeDistribution::stream_dump() {
  for (const auto &p : window_obj_size_req_cnt_) {
    ofs_stream_req << p << ",";
  }
  ofs_stream_req << "\n";

  for (const auto &p : window_obj_size_obj_cnt_) {
    ofs_stream_obj << p << ",";
  }
  ofs_stream_obj << "\n";
}
};  // namespace traceAnalyzer
