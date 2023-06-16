
#pragma once

#include <cmath>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../include/libCacheSim/macro.h"
#include "../include/libCacheSim/reader.h"
#include "struct.h"

namespace traceAnalyzer {
class SizeDistribution {
  /**
   * track a few things
   * 1. obj_size request count
   * 2. obj_size object  count
   * 3. obj_size request count over time (heatmap plot)
   * 4. obj_size object  count over time (heatmap plot)
   */
 public:
  SizeDistribution() = default;
  explicit SizeDistribution(std::string &output_path, int time_window)
      : time_window_(time_window) {
    obj_size_req_cnt_.reserve(1e6);
    obj_size_obj_cnt_.reserve(1e6);

    turn_on_stream_dump(output_path);
  };

  ~SizeDistribution() {
    ofs_stream_req.close();
    ofs_stream_obj.close();
  }

  void add_req(request_t *req);

  void dump(std::string &path_base);

 private:
  /* request/object count of certain size, size->count */
  std::unordered_map<obj_size_t, uint32_t> obj_size_req_cnt_;
  std::unordered_map<obj_size_t, uint32_t> obj_size_obj_cnt_;

  /* used to plot size distribution heatmap */
  const double LOG_BASE = 1.5;
  const double log_log_base = log(LOG_BASE);
  const int time_window_ = 300;
  int64_t next_window_ts_ = -1;

  std::vector<uint32_t> window_obj_size_req_cnt_;
  std::vector<uint32_t> window_obj_size_obj_cnt_;

  std::ofstream ofs_stream_req;
  std::ofstream ofs_stream_obj;

  void turn_on_stream_dump(std::string &path_base);

  void stream_dump();
};

}  // namespace traceAnalyzer