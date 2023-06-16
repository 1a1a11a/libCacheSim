//
// Created by Juncheng Yang on 4/20/20.
//

#pragma once

#define __STDC_FORMAT_MACROS

#include <unistd.h>

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../include/libCacheSim/reader.h"
#include "accessPattern.h"
#include "createFutureReuseCCDF.h"
#include "lifetime.h"
#include "op.h"
#include "popularity.h"
#include "popularityDecay.h"
#include "probAtAge.h"
#include "reqRate.h"
#include "reuse.h"
#include "size.h"
#include "sizeChange.h"
#include "struct.h"
#include "ttl.h"

/* experimental module */
#include "experimental/scanDetector.h"

// /* deprecated module */
// #include "dep/writeFutureReuse.h"
// #include "dep/writeReuse.h"

using namespace std;

namespace analysis {
struct analysis_option {
  bool req_rate;
  bool access_pattern;
  bool size;
  bool reuse;
  bool popularity;

  bool popularity_decay;
  bool lifetime;

  bool create_future_reuse_ccdf;
  bool prob_at_age;

  bool size_change;
};

struct analysis_param {
  int track_n_popular;
  /* tracking the number of one-hit, ... X-hit wonders */
  int track_n_hit;
  int time_window;
  int warmup_time;
  /* use for accessPattern only */
  int sample_ratio;
};

static struct analysis_param default_param() {
  struct analysis_param param;
  param.track_n_popular = 20;
  param.track_n_hit = 20;
  param.time_window = 300;
  param.warmup_time = 86400;
  param.sample_ratio = 101;

  return param;
};

static struct analysis_option default_option() {
  struct analysis_option option = {0};
  option.req_rate = false;
  option.access_pattern = false;
  option.size = false;
  option.reuse = false;
  option.popularity = false;
  option.popularity_decay = false;
  option.create_future_reuse_ccdf = false;
  option.prob_at_age = false;
  option.size_change = false;
  option.lifetime = false;

  return option;
};

#define DEFAULT_PREALLOC_N_OBJ 1e8

class Analysis {
 public:
  explicit Analysis(reader_t *reader, string output_path,
                    struct analysis_option option, struct analysis_param params)
      : reader_(reader),
        output_path_(std::move(output_path)),
        option_(option),
        track_n_popular_(params.track_n_popular),
        track_n_hit_(params.track_n_hit),
        time_window_(params.time_window),
        warmup_time_(params.warmup_time) {
    if (warmup_time_ % time_window_ != 0) {
      /* the popularityDecay computation needs warmup time to be multiple of
       * time_window */
      ERROR("warmup time needs to be multiple of time_window\n");
      exit(1);
    }
    obj_map_.reserve(DEFAULT_PREALLOC_N_OBJ);

    op_stat_ = new OpStat();
    ttl_stat_ = new TtlStat();

    if (option.req_rate) {
      req_rate_stat_ = new ReqRate(time_window_);
    }
    if (option.access_pattern) {
      access_stat_ = new AccessPattern((int64_t)get_num_of_req(reader_),
                                       params.sample_ratio);
    }
    if (option.size) {
      size_stat_ = new SizeDistribution(output_path_, time_window_);
    }
    if (option.reuse) {
      reuse_stat_ = new ReuseDistribution(output_path_, time_window_);
    }
    if (option.popularity_decay) {
      popularity_decay_stat_ =
          new PopularityDecay(output_path_, time_window_, warmup_time_);
    }
    if (option.create_future_reuse_ccdf) {
      create_future_reuse_ = new CreateFutureReuseDistribution(warmup_time_);
    }
    if (option.prob_at_age) {
      prob_at_age_ = new ProbAtAge(time_window_, warmup_time_);
    }
    if (option.lifetime) {
      lifetime_stat_ = new LifetimeDistribution();
    }

    if (option.size_change) {
      size_change_distribution_ = new SizeChangeDistribution();
    }

    // scan_detector_ = new ScanDetector(reader_, output_path, 100);
  };

  ~Analysis() {
    delete op_stat_;
    delete ttl_stat_;
    delete req_rate_stat_;
    delete reuse_stat_;
    delete size_stat_;
    delete access_stat_;
    delete popularity_stat_;
    delete popularity_decay_stat_;

    delete prob_at_age_;
    delete lifetime_stat_;
    delete create_future_reuse_;
    delete size_change_distribution_;

    // delete write_reuse_stat_;
    // delete write_future_reuse_stat_;

    delete scan_detector_;

    if (n_hit_cnt_ != nullptr) {
      delete[] n_hit_cnt_;
    }
    if (popular_cnt_ != nullptr) {
      delete[] popular_cnt_;
    }
  };

  void run();

  friend ostream &operator<<(ostream &os, const Analysis &stat) {
    if (!stat.has_run_) {
      os << "trace stat has not been computed" << endl;
    } else {
      os << stat.stat_ss_.str() << std::endl;
    }
    return os;
  }

  /* params */
  int time_window_;
  int warmup_time_;
  int track_n_popular_;
  int track_n_hit_;

  /* stat */
  int64_t n_req_ = 0;

  /* number of one-hit, two-hit ... */
  uint64_t *n_hit_cnt_ = nullptr;
  /* the number of requests to the most, 2nd most ... popular object */
  uint64_t *popular_cnt_ = nullptr;

  int64_t start_ts_ = 0, end_ts_ = 0;
  /* request size and object size weighted by request and object */
  uint64_t sum_obj_size_req = 0, sum_obj_size_obj = 0;
  /* request size can be different from object size when only part of
   * an object is requested, we ignore for now */
  //  uint64_t sum_req_size_req = 0, sum_req_size_obj = 0;

  obj_info_map_type obj_map_;

 private:
  reader_t *reader_ = nullptr;
  bool has_run_ = false;
  stringstream stat_ss_;
  struct analysis_option option_;

  OpStat *op_stat_ = nullptr;
  TtlStat *ttl_stat_ = nullptr;
  ReqRate *req_rate_stat_ = nullptr;
  SizeDistribution *size_stat_ = nullptr;
  ReuseDistribution *reuse_stat_ = nullptr;
  AccessPattern *access_stat_ = nullptr;
  Popularity *popularity_stat_ = nullptr;
  PopularityDecay *popularity_decay_stat_ = nullptr;

  ProbAtAge *prob_at_age_ = nullptr;
  LifetimeDistribution *lifetime_stat_ = nullptr;

  // WriteReuseDistribution *write_reuse_stat_ = nullptr;
  CreateFutureReuseDistribution *create_future_reuse_ = nullptr;
  // WriteFutureReuseDistribution *write_future_reuse_stat_ = nullptr;
  SizeChangeDistribution *size_change_distribution_ = nullptr;
  ScanDetector *scan_detector_ = nullptr;

  string output_path_;

  void post_processing();

  string gen_stat_str();

  inline int time_to_window_idx(uint32_t rtime) { return rtime / time_window_; }
};

};  // namespace analysis
