#include "reqRate.h"


namespace traceAnalyzer {

using namespace std;

void ReqRate::add_req(request_t *req) {
  if (unlikely(next_window_ts_ == -1)) {
    next_window_ts_ = (int64_t)req->clock_time + time_window_;
  }

  assert(next_window_ts_ != -1);

  window_n_req_ += 1;
  window_n_byte_ += req->obj_size;
  if (req->first_seen_in_window) window_n_obj_ += 1;

  //    if (window_seen_obj_.find(req->obj_id) == window_seen_obj_.end()) {
  //      window_seen_obj_.insert(req->obj_id);
  //    }

  if (req->compulsory_miss) {
    window_compulsory_miss_obj_ += 1;
  }

  while (req->clock_time >= next_window_ts_) {
    req_rate_.push_back(window_n_req_);
    byte_rate_.push_back(window_n_byte_);
    obj_rate_.push_back(window_n_obj_);
    first_seen_obj_rate_.push_back(window_compulsory_miss_obj_);
    window_n_req_ = 0;
    window_n_byte_ = 0;
    window_n_obj_ = 0;
    window_compulsory_miss_obj_ = 0;
    //      window_seen_obj_.clear();
    next_window_ts_ += time_window_;
  }
}

void ReqRate::dump(const string &path_base) {
  ofstream ofs(path_base + ".reqRate_w" + to_string(time_window_),
               ios::out | ios::trunc);
  ofs << "# " << path_base << "\n";
  ofs << "# req rate - time window " << time_window_ << " second\n";
  for (auto &n_req : req_rate_) {
    ofs << n_req / time_window_ << ",";
  }
  ofs << "\n";

  ofs << "# byte rate - time window " << time_window_ << " second\n";
  for (auto &n_byte : byte_rate_) {
    ofs << n_byte / time_window_ << ",";
  }
  ofs << "\n";

  ofs << "# obj rate - time window " << time_window_ << " second\n";
  for (auto &n_obj : obj_rate_) {
    ofs << n_obj / time_window_ << ",";
  }
  ofs << "\n";

  ofs << "# first seen obj (cold miss) rate - time window " << time_window_
      << " second\n";
  for (auto &n_obj : first_seen_obj_rate_) {
    ofs << n_obj / time_window_ << ",";
  }
  ofs << "\n";

  ofs.close();
}

};  // namespace traceAnalyzer
