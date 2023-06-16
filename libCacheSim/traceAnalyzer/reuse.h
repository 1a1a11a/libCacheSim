
#pragma once
/* plot the reuse distribution */

#include <cmath>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "../include/libCacheSim/reader.h"
#include "struct.h"
#include "utils/include/utils.h"

namespace traceAnalyzer {
class ReuseDistribution {
 public:
  explicit ReuseDistribution(std::string output_path, int time_window = 300,
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

  void add_req(request_t *req);

  void dump(std::string &path_base);

 private:
  /* request count for reuse rtime/vtime */
  std::unordered_map<int32_t, uint32_t> reuse_rtime_req_cnt_;
  std::unordered_map<int32_t, uint32_t> reuse_vtime_req_cnt_;

  std::unordered_map<int32_t, uint32_t> reuse_rtime_req_cnt_read_;
  std::unordered_map<int32_t, uint32_t> reuse_rtime_req_cnt_write_;
  std::unordered_map<int32_t, uint32_t> reuse_rtime_req_cnt_delete_;

  /* used to plot reuse distribution heatmap */
  const double log_base_ = 1.5;
  const double log_log_base_ = log(log_base_);
  const int rtime_granularity_;
  const int vtime_granularity_;
  const int time_window_;
  int64_t next_window_ts_ = -1;

  std::vector<uint32_t> window_reuse_rtime_req_cnt_;
  std::vector<uint32_t> window_reuse_vtime_req_cnt_;

  std::ofstream stream_dump_rt_ofs;
  std::ofstream stream_dump_vt_ofs;

  void turn_on_stream_dump(std::string &path_base);

  void stream_dump_window_reuse_distribution();
};

}  // namespace traceAnalyzer
