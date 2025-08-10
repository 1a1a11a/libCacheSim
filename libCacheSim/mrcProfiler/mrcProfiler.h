#pragma once
#include <inttypes.h>

#include <vector>

#include "../dataStructure/hash/hash.h"
#include "../dataStructure/robin_hood.h"
#include "libCacheSim.h"
#include "libCacheSim/cache.h"
#include "libCacheSim/macro.h"
#include "libCacheSim/plugin.h"
#include "libCacheSim/reader.h"
#include "libCacheSim/simulator.h"

namespace mrcProfiler {

#define MAX_MRC_PROFILE_POINTS 128

typedef enum {
  SHARDS_PROFILER,
  MINISIM_PROFILER,

  INVALID_PROFILER
} mrc_profiler_e;

/**
 * @brief get hash value for a 64-bit integer.
 */
static uint64_t get_hash_value_int_64_with_salt(uint64_t obj_id,
                                                uint64_t salt) {
  int64_t key = obj_id ^ salt;
  return get_hash_value_int_64(&key);
}

typedef struct profiler_params {
  struct {
    bool enable_fix_size;
    int64_t sample_size;
    double sample_rate;
    int64_t salt;

    void print() {
      printf("shards params:\n");
      printf("  enable_fix_size: %d\n", enable_fix_size);
      printf("  sample_size: %lld\n", (long long)sample_size);
      printf("  sample_rate: %f\n", sample_rate);
      printf("  salt: %lld\n", (long long)salt);
    }

    void parse_params(const char *str) {
      // format: FIX_RATE,0.01,hash_salt|FIX_SIZE,8192,hash_salt
      if (strlen(str) == 0) {
        ERROR("invalid params for shards\n");
        exit(1);
      }

      char buffer[1024];
      char *start = (char *)str;
      char *end = (char *)str;
      int current_param_idx = 0;
      while (*end != '\0') {
        end++;
        if (*end == ',' || *end == '\0') {
          // copy from start to end to buffer
          int need_size = end - start;
          if (need_size > 1024) {
            ERROR("params too long for shards: %s\n", str);
            exit(1);
          }
          memcpy(buffer, start, end - start);
          buffer[end - start] = '\0';

          if (current_param_idx == 0) {
            // check the sample type
            if (strcmp(buffer, "FIX_SIZE") == 0) {
              enable_fix_size = true;
            } else if (strcmp(buffer, "FIX_RATE") == 0) {
              enable_fix_size = false;
            } else {
              ERROR("invalid sample type for shards: %s\n", str);
              exit(1);
            }
          } else if (current_param_idx == 1) {
            // check the sample rate or sample size
            if (enable_fix_size) {
              sample_size = atoi(buffer);
              if (sample_size <= 0) {
                ERROR("invalid sample size for shards: %s\n", str);
                exit(1);
              }
            } else {
              sample_rate = atof(buffer);
              if (sample_rate <= 0 || sample_rate > 1) {
                ERROR("invalid sample rate for shards: %s\n", str);
                exit(1);
              }
            }
          } else if (current_param_idx == 2) {
            // check the salt
            salt = atoi(buffer);
          } else {
            ERROR("too many params for shards: %s\n", str);
            exit(1);
          }

          current_param_idx++;

          start = end + 1;
        }
      }
    }
  } shards_params;

  struct {
    double sample_rate;
    int64_t thread_num;

    void print() {
      printf("minisim params:\n");
      printf("  sample_rate: %f\n", sample_rate);
      printf("  thread_num: %lld\n", (long long)thread_num);
    }

    void parse_params(const char *str) {
      // format: FIX_RATE,0.01,thread_num
      if (strlen(str) == 0) {
        ERROR("invalid params for shards\n");
        exit(1);
      }

      char buffer[1024];
      char *start = (char *)str;
      char *end = (char *)str;
      int current_param_idx = 0;
      while (*end != '\0') {
        end++;
        if (*end == ',' || *end == '\0') {
          // copy from start to end to buffer
          int need_size = end - start;
          if (need_size > 1024) {
            ERROR("params too long for shards: %s\n", str);
            exit(1);
          }
          memcpy(buffer, start, end - start);
          buffer[end - start] = '\0';

          if (current_param_idx == 0) {
            // check the sample type
            if (strcmp(buffer, "FIX_RATE") == 0) {
              ;
            } else {
              ERROR("invalid sample type for minisim: %s\n", str);
              exit(1);
            }
          } else if (current_param_idx == 1) {
            // check the sample rate or sample size
            sample_rate = atof(buffer);
            if (sample_rate <= 0 || sample_rate > 1) {
              ERROR("invalid sample rate for minisim: %s\n", str);
              exit(1);
            }
          } else if (current_param_idx == 2) {
            // check the thread_num
            thread_num = atoi(buffer);
            if (thread_num <= 0) {
              ERROR("invalid thread_num for minisim: %s\n", str);
              exit(1);
            }
          } else {
            ERROR("too many params for minisim: %s\n", str);
            exit(1);
          }

          current_param_idx++;

          start = end + 1;
        }
      }
    }
  } minisim_params;

  std::vector<size_t> profile_size;
  std::vector<double> profile_wss_ratio;
  const char *cache_algorithm_str;

} mrc_profiler_params_t;

class MRCProfilerBase {
 public:
  MRCProfilerBase(reader_t *reader, std::string output_path,
                  const mrc_profiler_params_t &params)
      : reader_(reader),
        output_path_(std::move(output_path)),
        params_(params),
        mrc_size_vec(params.profile_size),
        hit_cnt_vec(params.profile_size.size(), 0),
        hit_size_vec(params.profile_size.size(), 0) {}

  virtual ~MRCProfilerBase() = default;

  /**
   * run the profiler, and store the result to hit_cnt_vec and hit_size_vec
   */
  virtual void run() = 0;

  /**
   * print the result to output_path
   *
   * @param output_path: if nullptr, use stdout
   */
  void print(const char *output_path = nullptr);

  size_t get_n_req() { return n_req_; }
  size_t get_sum_obj_size_req() { return sum_obj_size_req; }
  std::vector<size_t> get_mrc_size_vec() { return mrc_size_vec; }
  std::vector<int64_t> get_hit_cnt_vec() { return hit_cnt_vec; }
  std::vector<int64_t> get_hit_size_vec() { return hit_size_vec; }

 protected:
  reader_t *reader_ = nullptr;
  std::string output_path_;
  mrc_profiler_params_t params_;
  bool has_run_ = false;
  const char *profiler_name_ = nullptr;

  size_t n_req_ = 0;
  size_t sum_obj_size_req = 0;
  std::vector<size_t> mrc_size_vec;
  std::vector<int64_t> hit_cnt_vec;
  std::vector<int64_t> hit_size_vec;
};

class MRCProfilerSHARDS : public MRCProfilerBase {
 public:
  explicit MRCProfilerSHARDS(reader_t *reader, std::string output_path,
                             const mrc_profiler_params_t &params)
      : MRCProfilerBase(reader, output_path, params) {
    profiler_name_ = "SHARDS";
  }

  void run() override;

 private:
  void fixed_sample_rate_run();

  void fixed_sample_size_run();
};

class MRCProfilerMINISIM : public MRCProfilerBase {
 public:
  explicit MRCProfilerMINISIM(reader_t *reader, std::string output_path,
                              const mrc_profiler_params_t &params)
      : MRCProfilerBase(reader, output_path, params) {
    profiler_name_ = "MINISIM";
  }

  void run() override;

 private:
  cache_stat_t *result = nullptr;
};

MRCProfilerBase *create_mrc_profiler(mrc_profiler_e type, reader_t *reader,
                                     std::string output_path,
                                     const mrc_profiler_params_t &params);

}  // namespace mrcProfiler
