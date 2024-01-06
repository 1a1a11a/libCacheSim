//
// Created by Juncheng on 6/5/21.
//

#include <algorithm>  // std::make_heap, std::pop_heap, std::push_heap, std::sort_heap
#include <vector>  // std::vector

#include "analyzer.h"
#include "utils/include/utils.h"

void traceAnalyzer::TraceAnalyzer::initialize() {
  obj_map_.reserve(DEFAULT_PREALLOC_N_OBJ);

  op_stat_ = new OpStat();

  if (option_.ttl) {
    ttl_stat_ = new TtlStat();
  }

  if (option_.req_rate) {
    req_rate_stat_ = new ReqRate(time_window_);
  }

  if (option_.access_pattern) {
    access_stat_ = new AccessPattern(access_pattern_sample_ratio_inv_);
  }

  if (option_.size) {
    size_stat_ = new SizeDistribution(output_path_, time_window_);
  }

  if (option_.reuse) {
    reuse_stat_ = new ReuseDistribution(output_path_, time_window_);
  }

  if (option_.popularity_decay) {
    popularity_decay_stat_ =
        new PopularityDecay(output_path_, time_window_, warmup_time_);
  }

  if (option_.create_future_reuse_ccdf) {
    create_future_reuse_ = new CreateFutureReuseDistribution(warmup_time_);
  }

  if (option_.prob_at_age) {
    prob_at_age_ = new ProbAtAge(time_window_, warmup_time_);
  }

  if (option_.lifetime) {
    lifetime_stat_ = new LifetimeDistribution();
  }

  if (option_.size_change) {
    size_change_distribution_ = new SizeChangeDistribution();
  }

  // scan_detector_ = new ScanDetector(reader_, output_path, 100);
}

void traceAnalyzer::TraceAnalyzer::cleanup() {
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
}

void traceAnalyzer::TraceAnalyzer::run() {
  if (has_run_) return;

  request_t *req = new_request();
  read_one_req(reader_, req);
  start_ts_ = req->clock_time;
  int32_t curr_time_window_idx = 0;
  int next_time_window_ts = time_window_;

  int64_t n = 0;
  /* going through the trace */
  do {
    DEBUG_ASSERT(req->obj_size != 0);

    // change real time to relative time
    req->clock_time -= start_ts_;

    while (req->clock_time >= next_time_window_ts) {
      curr_time_window_idx += 1;
      next_time_window_ts += time_window_;
    }

    if (curr_time_window_idx != time_to_window_idx(req->clock_time)) {
      ERROR(
          "The data is not ordered by time, please sort the trace first!"
          "Current time %ld requested object %lu, obj size %lu\n",
          (long)(req->clock_time + start_ts_), (unsigned long)req->obj_id,
          (long)req->obj_size);
    }

    DEBUG_ASSERT(curr_time_window_idx == time_to_window_idx(req->clock_time));

    n_req_ += 1;
    sum_obj_size_req += req->obj_size;

    auto it = obj_map_.find(req->obj_id);
    if (it == obj_map_.end()) {
      /* the first request to the object */
      req->compulsory_miss =
          true; /* whether the object is seen for the first time */
      req->overwrite = false;
      req->first_seen_in_window = true;
      req->create_rtime = (int32_t)req->clock_time;
      req->prev_size = -1;
      //      req->last_seen_window_idx = curr_time_window_idx;

      req->vtime_since_last_access = -1;
      req->rtime_since_last_access = -1;

      struct obj_info obj_info;
      obj_info.create_rtime = (int32_t)req->clock_time;
      obj_info.freq = 1;
      obj_info.obj_size = (obj_size_t)req->obj_size;
      obj_info.last_access_rtime = (int32_t)req->clock_time;
      obj_info.last_access_vtime = n_req_;

      obj_map_[req->obj_id] = obj_info;
      sum_obj_size_obj += req->obj_size;

    } else {
      req->compulsory_miss = false;
      req->first_seen_in_window =
          (time_to_window_idx(it->second.last_access_rtime) !=
           curr_time_window_idx);
      req->create_rtime = it->second.create_rtime;
      if (req->op == OP_SET || req->op == OP_REPLACE || req->op == OP_CAS) {
        req->overwrite = true;
      } else {
        req->overwrite = false;
      }
      req->vtime_since_last_access =
          (int64_t)n_req_ - it->second.last_access_vtime;
      req->rtime_since_last_access =
          (int64_t)(req->clock_time) - it->second.last_access_rtime;

      assert(req->vtime_since_last_access > 0);
      assert(req->rtime_since_last_access >= 0);

      req->prev_size = it->second.obj_size;
      it->second.obj_size = req->obj_size;
      it->second.freq += 1;
      it->second.last_access_vtime = n_req_;
      it->second.last_access_rtime = (int32_t)(req->clock_time);
    }

    op_stat_->add_req(req);

    if (ttl_stat_ != nullptr) {
      ttl_stat_->add_req(req);
    }

    if (req_rate_stat_ != nullptr) {
      req_rate_stat_->add_req(req);
    }

    if (size_stat_ != nullptr) {
      size_stat_->add_req(req);
    }

    if (reuse_stat_ != nullptr) {
      reuse_stat_->add_req(req);
    }

    if (access_stat_ != nullptr) {
      access_stat_->add_req(req);
    }

    if (popularity_decay_stat_ != nullptr) {
      popularity_decay_stat_->add_req(req);
    }

    if (prob_at_age_ != nullptr) {
      prob_at_age_->add_req(req);
    }

    if (lifetime_stat_ != nullptr) {
      lifetime_stat_->add_req(req);
    }

    if (create_future_reuse_ != nullptr) {
      create_future_reuse_->add_req(req);
    }

    if (size_change_distribution_ != nullptr) {
      size_change_distribution_->add_req(req);
    }
    if (scan_detector_ != nullptr) {
      scan_detector_->add_req(req);
    }

    read_one_req(reader_, req);
  } while (req->valid);
  end_ts_ = req->clock_time + start_ts_;

  /* processing */
  post_processing();

  free_request(req);

  ofstream ofs("stat", ios::out | ios::app);
  ofs << gen_stat_str() << endl;
  ofs.close();

  if (ttl_stat_ != nullptr) {
    ttl_stat_->dump(output_path_);
  }

  if (req_rate_stat_ != nullptr) {
    req_rate_stat_->dump(output_path_);
  }

  if (reuse_stat_ != nullptr) {
    reuse_stat_->dump(output_path_);
  }

  if (size_stat_ != nullptr) {
    size_stat_->dump(output_path_);
  }

  if (access_stat_ != nullptr) {
    access_stat_->dump(output_path_);
  }

  if (popularity_stat_ != nullptr) {
    popularity_stat_->dump(output_path_);
  }

  if (popularity_decay_stat_ != nullptr) {
    popularity_decay_stat_->dump(output_path_);
  }

  if (prob_at_age_ != nullptr) {
    prob_at_age_->dump(output_path_);
  }

  if (lifetime_stat_ != nullptr) {
    lifetime_stat_->dump(output_path_);
  }

  if (create_future_reuse_ != nullptr) {
    create_future_reuse_->dump(output_path_);
  }

  // if (write_reuse_stat_ != nullptr) {
  //   write_reuse_stat_->dump(output_path_);
  // }

  // if (write_future_reuse_stat_ != nullptr) {
  //   write_future_reuse_stat_->dump(output_path_);
  // }

  if (scan_detector_ != nullptr) {
    scan_detector_->dump(output_path_);
  }

  has_run_ = true;
}

string traceAnalyzer::TraceAnalyzer::gen_stat_str() {
  stat_ss_.clear();
  double cold_miss_ratio = (double)obj_map_.size() / (double)n_req_;
  double byte_cold_miss_ratio =
      (double)sum_obj_size_obj / (double)sum_obj_size_req;
  int mean_obj_size_req = (int)((double)sum_obj_size_req / (double)n_req_);
  int mean_obj_size_obj =
      (int)((double)sum_obj_size_obj / (double)obj_map_.size());
  double freq_mean = (double)n_req_ / (double)obj_map_.size();
  int64_t time_span = end_ts_ - start_ts_;

  stat_ss_ << setprecision(4) << fixed << "dat: " << reader_->trace_path << "\n"
           << "number of requests: " << n_req_
           << ", number of objects: " << obj_map_.size() << "\n"
           << "number of req GiB: " << (double)sum_obj_size_req / (double)GiB
           << ", number of obj GiB: " << (double)sum_obj_size_obj / (double)GiB
           << "\n"
           << "compulsory miss ratio (req/byte): " << cold_miss_ratio << "/"
           << byte_cold_miss_ratio << "\n"
           << "object size weighted by req/obj: " << mean_obj_size_req << "/"
           << mean_obj_size_obj << "\n"
           << "frequency mean: " << freq_mean << "\n";
  stat_ss_ << "time span: " << time_span << "("
           << (double)(end_ts_ - start_ts_) / 3600 / 24 << " day)\n";

  stat_ss_ << *op_stat_;
  if (ttl_stat_ != nullptr) {
    stat_ss_ << *ttl_stat_;
  }
  if (req_rate_stat_ != nullptr) stat_ss_ << *req_rate_stat_;
  if (popularity_stat_ != nullptr) stat_ss_ << *popularity_stat_;

  stat_ss_ << "X-hit (number of obj accessed X times): ";
  for (int i = 0; i < track_n_hit_; i++) {
    stat_ss_ << n_hit_cnt_[i] << "("
             << (double)n_hit_cnt_[i] / (double)obj_map_.size() << "), ";
  }
  stat_ss_ << "\n";

  stat_ss_ << "freq (fraction) of the most popular obj: ";
  for (int i = 0; i < track_n_popular_; i++) {
    stat_ss_ << popular_cnt_[i] << "("
             << (double)popular_cnt_[i] / (double)n_req_ << "), ";
  }
  stat_ss_ << "\n";

  if (size_change_distribution_ != nullptr)
    stat_ss_ << *size_change_distribution_;

  if (scan_detector_ != nullptr) stat_ss_ << *scan_detector_;

  return stat_ss_.str();
}

void traceAnalyzer::TraceAnalyzer::post_processing() {
  assert(n_hit_cnt_ == nullptr);
  assert(popular_cnt_ == nullptr);

  n_hit_cnt_ = new uint64_t[track_n_hit_];
  popular_cnt_ = new uint64_t[track_n_popular_];
  memset(n_hit_cnt_, 0, sizeof(uint64_t) * track_n_hit_);
  memset(popular_cnt_, 0, sizeof(uint64_t) * track_n_popular_);

  for (auto it : obj_map_) {
    if (it.second.freq <= track_n_hit_) {
      n_hit_cnt_[it.second.freq - 1] += 1;
    }
  }

  if (option_.popularity) {
    popularity_stat_ = new Popularity(obj_map_);
    auto sorted_freq = popularity_stat_->get_sorted_freq();
    for (int i = 0; i < track_n_popular_; i++) {
      popular_cnt_[i] = sorted_freq[i];
    }
  }
}
