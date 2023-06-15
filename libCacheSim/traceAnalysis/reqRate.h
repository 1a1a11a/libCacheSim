

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <unordered_set>


#include "../include/libCacheSim/request.h"
#include "../include/libCacheSim/macro.h"


using namespace std;

namespace analysis {

class ReqRate {
 public:
  ReqRate() = default;
  explicit ReqRate(int time_window): time_window_(time_window) {};
  ~ReqRate() = default;

  inline void add_req(request_t *req) {

    if (unlikely(next_window_ts_ == -1)) {
      next_window_ts_ = (int64_t) req->clock_time + time_window_;
    }

    assert(next_window_ts_ != -1);

    window_n_req_ += 1;
    window_n_byte_ += req->obj_size;
    if (req->first_seen_in_window)
      window_n_obj_ += 1;

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

  void dump(const string& path_base) {
    ofstream ofs(path_base + ".reqRate_w" + to_string(time_window_), ios::out|ios::trunc);
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

    ofs << "# first seen obj (cold miss) rate - time window " << time_window_ << " second\n";
    for (auto &n_obj : first_seen_obj_rate_) {
      ofs << n_obj / time_window_ << ",";
    }
    ofs << "\n";

    ofs.close();
  }

  friend ostream &operator<<(ostream &os, const ReqRate &rr) {
    if (rr.req_rate_.size() < 10) {
      WARN("request rate not enough window (%zu window)\n", rr.req_rate_.size());
      if (rr.req_rate_.empty())
        return os;
    }
    double max = (double) *std::max_element(rr.req_rate_.cbegin(), rr.req_rate_.cend()) / rr.time_window_;
    double min = (double) *std::min_element(rr.req_rate_.cbegin(), rr.req_rate_.cend()) / rr.time_window_;
    os << fixed << "request rate min " << min << " req/s, max " << max << " req/s, window " << rr.time_window_ << "s\n";

    max = (double) *std::max_element(rr.obj_rate_.cbegin(), rr.obj_rate_.cend()) / rr.time_window_;
    min = (double) *std::min_element(rr.obj_rate_.cbegin(), rr.obj_rate_.cend()) / rr.time_window_;
    os << fixed << "object rate min " << min << " obj/s, max " << max << " obj/s, window " << rr.time_window_ << "s\n";

    return os;
  }

 private:
  const int time_window_ = 60;
  /* objects have been seen in the time window */
//  unordered_set<obj_id_t> window_seen_obj_{};
  int64_t next_window_ts_ = -1;
  /* how many requests in the window */
  uint32_t window_n_req_ = 0;
  /* how many bytes in the window */
  uint64_t window_n_byte_ = 0;
  /* how many objects in the window */
  uint32_t window_n_obj_ = 0;
  /* how many objects requested in the window are first-time in the trace */
  uint32_t window_compulsory_miss_obj_ = 0;
  vector<uint32_t> req_rate_{};
  vector<uint64_t> byte_rate_{}; /* bytes/sec */
  vector<uint32_t> obj_rate_{};
  /* used to calculate cold miss ratio over time */
  vector<uint32_t> first_seen_obj_rate_{};

};
}