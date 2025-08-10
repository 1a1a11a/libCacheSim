#include "./mrcProfiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "../dataStructure/minvaluemap.hpp"
#include "../dataStructure/splaytree.hpp"
#include "libCacheSim/const.h"

mrcProfiler::MRCProfilerBase *mrcProfiler::create_mrc_profiler(
    mrc_profiler_e type, reader_t *reader, std::string output_path,
    const mrc_profiler_params_t &params) {
  switch (type) {
    case mrc_profiler_e::SHARDS_PROFILER:
      return new MRCProfilerSHARDS(reader, output_path, params);
    case mrc_profiler_e::MINISIM_PROFILER:
      return new MRCProfilerMINISIM(reader, output_path, params);
    default:
      ERROR("unknown profiler type %d\n", type);
      exit(1);
  }
}

void mrcProfiler::MRCProfilerBase::print(const char *output_path) {
  if (!has_run_) {
    ERROR("MRCProfiler has not been run\n");
    return;
  }

  FILE *outfp = stdout;
  bool open_output_file = false;
  if (output_path != nullptr && strlen(output_path) != 0) {
    outfp = fopen(output_path, "w");
    open_output_file = true;
    if (outfp == nullptr) {
      WARN("failed to open file %s\n", output_path);
      outfp = stdout;
      open_output_file = false;
    }
  }

  fprintf(outfp, "profiler: %s\n", profiler_name_);
  fprintf(outfp, "trace: %s\n", reader_->trace_path);
  fprintf(outfp, "cache_algorithm: %s\n", params_.cache_algorithm_str);
  fprintf(outfp, "n_req: %ld\n", n_req_);
  fprintf(outfp, "sum_obj_size_req: %ld\n", sum_obj_size_req);

  if (params_.profile_wss_ratio.size() != 0) {
    fprintf(outfp, "wss_ratio\t");
  }
  fprintf(outfp, "cache_size\tmiss_rate\tbyte_miss_rate\n");
  for (size_t i = 0; i < mrc_size_vec.size(); i++) {
    if (params_.profile_wss_ratio.size() != 0) {
      fprintf(outfp, "%lf\t", params_.profile_wss_ratio[i]);
    }
    double miss_rate = 1 - (double)hit_cnt_vec[i] / (n_req_);
    double byte_miss_rate = 1 - (double)hit_size_vec[i] / (sum_obj_size_req);

    // clip to [0, 1]
    miss_rate = miss_rate > 1 ? 1 : (miss_rate < 0 ? 0 : miss_rate);
    byte_miss_rate =
        byte_miss_rate > 1 ? 1 : (byte_miss_rate < 0 ? 0 : byte_miss_rate);
    fprintf(outfp, "%ldB\t%lf\t%lf\n", mrc_size_vec[i], miss_rate,
            byte_miss_rate);
  }

  if (open_output_file) {
    fclose(outfp);
  }
}

void mrcProfiler::MRCProfilerSHARDS::run() {
  if (has_run_) return;

  if (params_.shards_params.enable_fix_size) {
    fixed_sample_size_run();
  } else {
    fixed_sample_rate_run();
  }

  has_run_ = true;
}

void mrcProfiler::MRCProfilerSHARDS::fixed_sample_rate_run() {
  // 1. init
  request_t *req = new_request();
  double sample_rate = params_.shards_params.sample_rate;
  std::vector<double> local_hit_cnt_vec(mrc_size_vec.size(), 0);
  std::vector<double> local_hit_size_vec(mrc_size_vec.size(), 0);
  uint64_t sample_max = UINT64_MAX * sample_rate;
  if (sample_rate == 1) {
    INFO("sample_rate is 1, no need to sample\n");
    sample_max = UINT64_MAX;
  }
  double sampled_cnt = 0, sampled_size = 0;
  int64_t current_time = 0;
  robin_hood::unordered_map<obj_id_t, int64_t> last_access_time_map;
  SplayTree<int64_t, uint64_t> rd_tree;

  // 2. go through the trace
  read_one_req(reader_, req);
  /* going through the trace */
  do {
    DEBUG_ASSERT(req->obj_size != 0);
    n_req_ += 1;
    sum_obj_size_req += req->obj_size;

    uint64_t hash_value = get_hash_value_int_64_with_salt(
        req->obj_id, params_.shards_params.salt);
    current_time += 1;
    if (hash_value <= sample_max) {
      sampled_cnt += 1.0 / sample_rate;
      sampled_size += 1.0 * req->obj_size / sample_rate;

      if (last_access_time_map.count(req->obj_id)) {
        int64_t last_access_time = last_access_time_map[req->obj_id];
        size_t stack_distance =
            rd_tree.getDistance(last_access_time) / sample_rate;

        last_access_time_map[req->obj_id] = current_time;

        // update tree
        rd_tree.erase(last_access_time);
        rd_tree.insert(current_time, req->obj_size);

        // find bucket to increase hit cnt and hit size
        auto it = std::lower_bound(mrc_size_vec.begin(), mrc_size_vec.end(),
                                   stack_distance);

        if (it != mrc_size_vec.end()) {
          // update hit cnt and hit size
          int idx = std::distance(mrc_size_vec.begin(), it);
          local_hit_cnt_vec[idx] += 1.0 / sample_rate;
          local_hit_size_vec[idx] += 1.0 * req->obj_size / sample_rate;
        }

      } else {
        last_access_time_map[req->obj_id] = current_time;
        // update the tree
        rd_tree.insert(current_time, req->obj_size);
      }
    }

    read_one_req(reader_, req);
  } while (req->valid);

  // 3. adjust the hit cnt and hit size
  local_hit_cnt_vec[0] += n_req_ - sampled_cnt;
  local_hit_size_vec[0] += sum_obj_size_req - sampled_size;

  free_request(req);

  // 4. calculate the mrc
  int64_t accu_hit_cnt = 0, accu_hit_size = 0;
  for (size_t i = 0; i < mrc_size_vec.size(); i++) {
    accu_hit_cnt += local_hit_cnt_vec[i];
    accu_hit_size += local_hit_size_vec[i];
    hit_cnt_vec[i] = accu_hit_cnt;
    hit_size_vec[i] = accu_hit_size;
  }
}

void mrcProfiler::MRCProfilerSHARDS::fixed_sample_size_run() {
  // 1. init
  request_t *req = new_request();
  double sample_rate = 1.0;
  std::vector<double> local_hit_cnt_vec(mrc_size_vec.size(), 0);
  std::vector<double> local_hit_size_vec(mrc_size_vec.size(), 0);
  double sampled_cnt = 0, sampled_size = 0;
  int64_t current_time = 0;
  int64_t max_to_keep = params_.shards_params.sample_size;

  MinValueMap<int64_t, uint64_t> min_value_map(max_to_keep);
  robin_hood::unordered_map<obj_id_t, int64_t> last_access_time_map;
  SplayTree<int64_t, uint64_t> rd_tree;

  // 2. go through the trace
  read_one_req(reader_, req);
  /* going through the trace */
  do {
    DEBUG_ASSERT(req->obj_size != 0);
    n_req_ += 1;
    sum_obj_size_req += req->obj_size;

    uint64_t hash_value = get_hash_value_int_64_with_salt(
        req->obj_id, params_.shards_params.salt);

    current_time += 1;
    if (!min_value_map.full() || hash_value < min_value_map.get_max_value() ||
        last_access_time_map.count(req->obj_id)) {
      // this is a sampled req

      if (!last_access_time_map.count(req->obj_id)) {
        bool poped = false;
        int64_t poped_id = min_value_map.insert(req->obj_id, hash_value, poped);
        if (poped) {
          // this is a sampled req
          int64_t poped_id_access_time = last_access_time_map[poped_id];
          rd_tree.erase(poped_id_access_time);
          last_access_time_map.erase(poped_id);
        }
      }

      if (!min_value_map.full()) {
        sample_rate = 1.0;  // still 100% sample rate
      } else {
        sample_rate = min_value_map.get_max_value() * 1.0 /
                      UINT64_MAX;  // adjust the sample rate
      }

      sampled_cnt += 1.0 / sample_rate;
      sampled_size += 1.0 * req->obj_size / sample_rate;

      if (last_access_time_map.count(req->obj_id)) {
        int64_t last_acc_time = last_access_time_map[req->obj_id];
        int64_t stack_distance =
            rd_tree.getDistance(last_acc_time) * 1.0 / sample_rate;

        last_access_time_map[req->obj_id] = current_time;

        rd_tree.erase(last_acc_time);
        rd_tree.insert(current_time, req->obj_size);

        // find bucket to increase hit cnt and hit size
        auto it = std::lower_bound(mrc_size_vec.begin(), mrc_size_vec.end(),
                                   stack_distance);

        if (it != mrc_size_vec.end()) {
          // update hit cnt and hit size
          int idx = std::distance(mrc_size_vec.begin(), it);
          local_hit_cnt_vec[idx] += 1.0 / sample_rate;
          local_hit_size_vec[idx] += req->obj_size * 1.0 / sample_rate;
        }
      } else {
        last_access_time_map[req->obj_id] = current_time;
        rd_tree.insert(current_time, req->obj_size);
      }
    }

    read_one_req(reader_, req);
  } while (req->valid);

  // 3. adjust the hit cnt and hit size
  local_hit_cnt_vec[0] += n_req_ - sampled_cnt;
  local_hit_size_vec[0] += sum_obj_size_req - sampled_size;

  free_request(req);

  // 4. calculate the mrc
  int64_t accu_hit_cnt = 0, accu_hit_size = 0;
  for (size_t i = 0; i < mrc_size_vec.size(); i++) {
    accu_hit_cnt += local_hit_cnt_vec[i];
    accu_hit_size += local_hit_size_vec[i];
    hit_cnt_vec[i] = accu_hit_cnt;
    hit_size_vec[i] = accu_hit_size;
  }
}

void mrcProfiler::MRCProfilerMINISIM::run() {
  has_run_ = true;

  request_t *req = new_request();
  double sample_rate = params_.minisim_params.sample_rate;
  double sampled_cnt = 0, sampled_size = 0;
  sampler_t *sampler = nullptr;
  if (sample_rate > 0.5) {
    INFO("sample_rate is too large, do not sample\n");
  } else {
    sampler = create_spatial_sampler(sample_rate);
    set_spatial_sampler_salt(sampler,
                             10000019);  // TODO: salt can be changed by params
  }

  // 1. obtain the n_req_, sum_obj_size_req, sampled_cnt and sampled_size
  read_one_req(reader_, req);
  do {
    DEBUG_ASSERT(req->obj_size != 0);
    n_req_ += 1;
    sum_obj_size_req += req->obj_size;
    if (sampler == nullptr || sampler->sample(sampler, req)) {
      sampled_cnt += 1;
      sampled_size += req->obj_size;
    }

    read_one_req(reader_, req);
  } while (req->valid);
  // 2. set spatial sampling to the reader
  reset_reader(reader_);
  reader_->init_params.sampler = sampler;
  reader_->sampler = sampler;

  // 3. run the simulate_with_multi_caches
  cache_t *caches[MAX_MRC_PROFILE_POINTS];
  for (size_t i = 0; i < params_.profile_size.size(); i++) {
    size_t _cache_size = mrc_size_vec[i] * sample_rate;
    common_cache_params_t cc_params = {.cache_size = _cache_size,
                                       .default_ttl = 0,
                                       .hashpower = 20,
                                       .consider_obj_metadata = false};
    caches[i] = create_cache_using_plugin(params_.cache_algorithm_str,
                                          cc_params, nullptr);
  }
  result = simulate_with_multi_caches(
      reader_, caches, mrc_size_vec.size(), NULL, 0, 0,
      params_.minisim_params.thread_num, true, true);

  // 4. adjust hit cnt and hit size
  for (size_t i = 0; i < mrc_size_vec.size(); i++) {
    if (sampler) {
      hit_cnt_vec[i] =
          n_req_ - result[i].n_miss * reader_->sampler->sampling_ratio_inv;
      hit_size_vec[i] =
          sum_obj_size_req -
          result[i].n_miss_byte * reader_->sampler->sampling_ratio_inv;
    } else {
      hit_cnt_vec[i] = n_req_ - result[i].n_miss;
      hit_size_vec[i] = sum_obj_size_req - result[i].n_miss_byte;
    }
  }
  // clean up
  my_free(sizeof(cache_stat_t) * mrc_size_vec.size(), result);
  free_request(req);
}
