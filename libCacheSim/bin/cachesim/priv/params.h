#pragma once

#include "../cachesim.h"
#include "../utils.h"
#include "math.h"
#include "cphy.h"

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
  args->per_obj_metadata = 0;

  args->debug = false; 

#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
  args->obj_id_type = OBJ_ID_NUM;

  args->seg_size = 200;
  args->n_merge = 2;
  args->rank_intvl = 0.02;
  args->merge_consecutive_segs = true;
  args->train_source_x = TRAIN_X_FROM_SNAPSHOT;
  args->train_source_y = TRAIN_Y_FROM_ONLINE;

  args->age_shift = 2; 
  args->retrain_intvl = 86400;

  args->L2Cache_type = LOGCACHE_LEARNED;
  args->bucket_type = NO_BUCKET;
#endif


  args->n_thread = 4;
  args->n_thread = (int) n_cores();
}

static inline void set_param_with_workload(sim_arg_t *args, char *trace_path) {
  args->trace_path = trace_path;

#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
  if (strstr(args->trace_path, "cphy") != NULL) {
    uint64_t s[10] = {500, 1000, 2000, 4000, 8000, 12000, 16000, 24000, 32000, 64000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);

    // if (!set_cphy_size(args)) {
    //   INFO("use cphy default parameter\n");
    // }

    args->retrain_intvl = 86400 * 1;
  } else if (strstr(args->trace_path, "media_metadata") != NULL) {
    /* media_metadata */
    uint64_t s[11] = {100, 200, 300, 400, 500, 600, 800, 1000, 1200, 1600, 2000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = 11;
    args->seg_size = 1000;
    //    args->bucket_type = SIZE_BUCKET;
    args->retrain_intvl = 3600 * 4;

  } else if (strstr(args->trace_path, "user_activity") != NULL) {
    /* user activity */
    uint64_t s[10] = {200, 500, 1000, 1500, 2000, 3000, 4000, 5000, 6000, 8000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    /* 2GB 8 MQPS 0.0509 on full trace */
    args->n_cache_size = 10;
    args->seg_size = 1000;
    args->bucket_type = SIZE_BUCKET;

    /* use LHD for object selection */
    //    args->retrain_intvl = 3600 * 8;
    /* use 1-4 when for rank:map */
    args->retrain_intvl = 3600 * 2;
  } else if (strstr(args->trace_path, "nyc") != NULL
             || strstr(args->trace_path, "wiki") != NULL) {
    /* nyc */
    uint64_t s[8] = {20, 50, 100, 200, 400, 500, 800, 1000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = GiB * s[i];
    }
    args->n_cache_size = 8;
    args->seg_size = 200;
    //    args->seg_size = 10;
    args->age_shift = 4;
    args->bucket_type = NO_BUCKET;
    // args->size_bucket_base = 1000;
    args->min_start_train_seg = 10000;
    args->max_start_train_seg = 80000;
    args->n_train_seg_growth = 20000;
    args->sample_every_n_seg_for_training = 4;
    args->retrain_intvl = 86400;

  } else if (strstr(args->trace_path, "sjc") != NULL
             || strstr(args->trace_path, "lax") != NULL) {
    /* nyc */
    uint64_t s[14] = {50,   100,  200,  400,  500,  800,  1000,
                      1500, 2000, 3000, 4000, 5000, 6000, 8000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = GiB * s[i];
    }
    args->n_cache_size = 14;
  } else if (strstr(args->trace_path, "cluster") != NULL) {
    uint64_t s[9] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
  } else {
    printf("cannot detect trace name\n");
    uint64_t s[7] = {50, 100, 400, 1000, 2000, 4000, 8000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = 8;
    args->seg_size = 4000;
    args->bucket_type = NO_BUCKET;
    args->age_shift = 2;

    //    abort();
  }
#endif
}
