#pragma once

#include "../cachesim.h"
#include "../utils.h"
#include "math.h"

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

static inline void set_default_arg(sim_arg_t *args) {
  uint64_t s[8] = {10, 50, 100, 400, 1000, 2000, 4000, 8000};
  for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
    args->cache_sizes[i] = MiB * s[i];
  }
  args->n_cache_size = 8;
  args->cache_size_in_mb = 1024 * MiB;
  args->per_obj_overhead = 0;

  args->debug = false;

#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
  args->obj_id_type = OBJ_ID_NUM;

  args->seg_size = 200;
  args->n_merge = 2;
  args->rank_intvl = 0.02;
  args->merge_consecutive_segs = true;
  args->train_source_y = TRAIN_Y_FROM_ONLINE;

  args->age_shift = 2;
  args->retrain_intvl = 86400;

  args->L2Cache_type = LOGCACHE_LEARNED;
  args->bucket_type = NO_BUCKET;
#endif

  args->n_thread = (int)n_cores();
}

static inline void set_param_with_workload(sim_arg_t *args, char *trace_path) {
  args->trace_path = trace_path;

#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
  if (strstr(args->trace_path, "cphy") != NULL ||
      strstr(args->trace_path, "msr") != NULL) {
    uint64_t s[10] = {500,   1000,  2000,  4000,  8000,
                      12000, 16000, 24000, 32000, 64000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
    args->retrain_intvl = 86400 * 1;
  } else if (strstr(args->trace_path, "wiki") != NULL) {
    uint64_t s[12] = {10,  20,   50,   100,  200,  400,
                      800, 1200, 1600, 2400, 3200, 6400};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = s[i] * 1000 * MiB;
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
  } else if (strstr(args->trace_path, "cluster") != NULL) {
    uint64_t s[9] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
  } else {
    printf("cannot detect trace name\n");
    uint64_t s[8] = {50, 100, 400, 1000, 2000, 4000, 8000, 16000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
  }
#endif
}
