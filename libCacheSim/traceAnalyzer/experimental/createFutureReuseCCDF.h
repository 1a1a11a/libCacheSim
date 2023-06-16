
#pragma once
/* calculate the reuse CCDF at certain create age, each point (x, y) in the
 * distribution measures the probability of having future reuse y at age x */

#include <assert.h>
#include <unordered_map>
#include <unordered_set>

#include "../../include/libCacheSim/reader.h"
#include "../struct.h"
#include "../utils/include/utils.h"

namespace traceAnalyzer {

class CreateFutureReuseDistribution {
 public:
  explicit CreateFutureReuseDistribution(int warmup_time = 7200)
      : warmup_time_(warmup_time) {
    assert(reuse_info_array_size_ ==
           max_reuse_rtime_ / rtime_granularity_ + fine_rtime_upbound_);
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

  void add_req(request_t *req);

  void dump(string &path_base);

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

}  // namespace traceAnalyzer
