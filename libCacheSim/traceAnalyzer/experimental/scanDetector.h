

#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../include/libCacheSim/macro.h"
#include "../../include/libCacheSim/reader.h"

using namespace std;

namespace traceAnalyzer {
class ScanDetector {
  struct ScanStream {
    int64_t stream_start_vtime;
    int64_t stream_next_start_vtime;
    int n_req;
    std::vector<int64_t> req_vtime;

    ScanStream(int64_t stream_start_vtime, int64_t stream_next_start_vtime)
        : stream_start_vtime(stream_start_vtime),
          stream_next_start_vtime(stream_next_start_vtime),
          n_req(1) {
      req_vtime.push_back(stream_start_vtime);
    }
  };

 public:
  ScanDetector() = delete;
  explicit ScanDetector(reader_t *reader, string &output_path,
                        int max_vtime_diff)
      : max_vtime_diff_(max_vtime_diff) {
    if (strcasestr(reader->trace_path, ".zst") != NULL) {
      printf("zstd trace is not supported yet\n");
      abort();
    }

    min_scan_size_ =
        getenv("MIN_SCAN_SIZE") ? atoi(getenv("MIN_SCAN_SIZE")) : 10;

    // next_access_vtime_deque_.resize(max_vtime_diff);
    is_scan_req_vec_.resize(get_num_of_req(reader), false);
    printf("min_scan_size: %d, %ld requests\n", min_scan_size_,
           is_scan_req_vec_.size());
  };

  ~ScanDetector() {}

  // void add_req0(request_t *req) {
  //     if (unlikely(req->next_access_vtime == -2)) {
  //         std::cout << "the trace does not oracle trace, scan detector needs
  //         oracle" << std::endl; abort();
  //     }

  //     bool is_scan_req = false;
  //     for (auto next_access_vtime : next_access_vtime_deque_) {
  //         if (req->next_access_vtime >= next_access_vtime - max_vtime_diff_
  //         && req->next_access_vtime <= next_access_vtime + max_vtime_diff_) {
  //             is_scan_req = true;
  //             break;
  //         }
  //     }

  // if (is_scan_req) {
  //     n_scan_req_ += 1;
  //     is_scan_req_vec_.push_back(true);
  // } else {
  //     n_non_scan_req_ += 1;
  //     is_scan_req_vec_.push_back(false);
  // }

  //     next_access_vtime_deque_.pop_front();
  //     next_access_vtime_deque_.push_back(req->next_access_vtime);
  // }

  inline bool is_part_of_scan(struct ScanStream &ss, int64_t curr_vtime,
                              int64_t next_access_vtime) {
    if (curr_vtime > ss.stream_start_vtime + max_vtime_diff_ + ss.n_req ||
        curr_vtime < ss.stream_start_vtime - max_vtime_diff_ - ss.n_req) {
      return false;
    }

    if (next_access_vtime <
            ss.stream_next_start_vtime - max_vtime_diff_ - ss.n_req ||
        next_access_vtime >
            ss.stream_next_start_vtime + max_vtime_diff_ + ss.n_req) {
      return false;
    }

    return true;
  }

  void add_req(request_t *req) {
    if (unlikely(req->next_access_vtime == -2)) {
      std::cout << "the trace does not oracle trace, scan detector needs oracle"
                << std::endl;
      abort();
    }

    bool is_scan_req = false;

    if (req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX) {
      n_non_scan_req_ += 1;
      curr_vtime += 1;
      return;
    }

    for (int i = 0; i < n_active_stream_; i++) {
      if (is_part_of_scan(scan_stream_vec_[i], curr_vtime,
                          req->next_access_vtime)) {
        scan_stream_vec_[i].n_req += 1;
        scan_stream_vec_[i].req_vtime.push_back(curr_vtime);
        is_scan_req = true;
        break;
      }
    }

    if (!is_scan_req) {
      int pos = 0;
      if (n_active_stream_ >= max_vtime_diff_ - 1) {
        while (pos < n_active_stream_) {
          if (curr_vtime > scan_stream_vec_[pos].stream_start_vtime +
                               max_vtime_diff_ + scan_stream_vec_[pos].n_req) {
            // if (scan_stream_vec_[pos].n_req > 1)
            //     printf("%d\n", scan_stream_vec_[pos].n_req);
            if (scan_stream_vec_[pos].n_req > min_scan_size_) {
              n_scan_req_ += scan_stream_vec_[pos].n_req;
              for (auto vtime : scan_stream_vec_[pos].req_vtime) {
                is_scan_req_vec_[vtime] = true;
              }
            } else {
              n_non_scan_req_ += scan_stream_vec_[pos].n_req;
            }

            scan_size_vec_.push_back(scan_stream_vec_[pos].n_req);

            scan_stream_vec_[pos].n_req = 0;
            scan_stream_vec_[pos] = scan_stream_vec_[n_active_stream_ - 1];
            scan_stream_vec_.pop_back();
            n_active_stream_ -= 1;
          }
          pos += 1;
        }
      }

      scan_stream_vec_.emplace_back(curr_vtime,
                                    (int64_t)req->next_access_vtime);
      n_active_stream_ += 1;
    }

    curr_vtime += 1;
    // if (curr_vtime % 100000 == 0)
    //     printf("%ld\n", curr_vtime);
  }

  void dump(string &path_base) {
    std::cout << "scanDetector: max_vtime_diff " << max_vtime_diff_ * 2
              << ", "
              //   << " min_scan_size " << min_scan_size_ << ", "
              << n_scan_req_ << "/" << n_non_scan_req_ << " scan/non_scan reqs "
              << (double)n_scan_req_ / (n_scan_req_ + n_non_scan_req_) * 100
              << "%" << std::endl;

    ofstream ofs_ss(path_base + ".scanSize", ios::out | ios::trunc);
    ofs_ss << "# " << path_base << "\n";
    ofs_ss << "# scan_size: \n";
    for (auto ss : scan_size_vec_) {
      ofs_ss << ss << "\n";
    }

    ofs_ss.close();

    ofstream ofs_is(path_base + ".scan", ios::out | ios::trunc);
    ofs_is << "# " << path_base << "\n";
    ofs_is << "# is_scan: \n";
    for (auto is_scan : is_scan_req_vec_) {
      if (is_scan)
        ofs_is << "1\n";
      else
        ofs_is << "0\n";
    }

    ofs_is.close();
  }

  friend ostream &operator<<(ostream &os, const ScanDetector &scan_detector) {
    os << "scanDetector: max_vtime_diff "
       << scan_detector.max_vtime_diff_ * 2
       //    << " min_scan_size " << scan_detector.min_scan_size_
       << scan_detector.n_scan_req_ << "/" << scan_detector.n_non_scan_req_
       << " scan/non_scan reqs" << std::endl;

    return os;
  }

 private:
  int max_vtime_diff_;
  int min_scan_size_ = 20;
  int64_t n_scan_req_ = 0;
  int64_t n_non_scan_req_ = 0;

  // record whether each request is a scan request
  // the vector size is the same as trace length
  std::vector<bool> is_scan_req_vec_;

  // we maintain a list of max_vtime_diff_ requests
  // each element records the next_access_vtime of the request
  // std::deque<int64_t> next_access_vtime_deque_;

  std::vector<ScanStream> scan_stream_vec_;
  int n_active_stream_ = 0;
  int64_t curr_vtime = 0;

  std::vector<int32_t> scan_size_vec_;
};

}  // namespace analysis
