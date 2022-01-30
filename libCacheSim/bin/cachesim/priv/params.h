#pragma once

#include "../cachesim.h"
#include "../utils.h"
#include "math.h"


#ifdef __linux__
#include <sys/sysinfo.h>
#endif


static inline void set_default_arg(sim_arg_t *args) {
#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
  args->obj_id_type = OBJ_ID_NUM;

  args->seg_size = 1000;
  args->n_merge = 2;
  args->rank_intvl = 20;
  args->age_shift = 0;
  args->min_start_train_seg = 1000;
  args->max_start_train_seg = 10000;
  args->n_train_seg_growth = 1000;
  args->sample_every_n_seg_for_training = 1;
  args->snapshot_intvl = 3600;
  args->retrain_intvl = 86400 * 2;

  args->lsc_type = LOGCACHE_LEARNED;
  args->bucket_type = NO_BUCKET;
  args->size_bucket_base = 1;
#endif

  args->n_thread = 4;

#ifdef __linux__
  //  printf("This system has %d processors configured and "
  //         "%d processors available.\n",
  //         get_nprocs_conf(), get_nprocs());
  args->n_thread = get_nprocs();
#endif

  args->n_thread = (int) n_cores();
  args->per_obj_metadata = 0; 
  args->debug = 0; 
}

static inline void set_param_with_workload(sim_arg_t *args) {
#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
  if (strstr(args->trace_path, "w105") != NULL) {
    /* w105 */
    uint64_t s[7] = {1000, 2000, 4000, 8000, 10000, 12000, 16000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = 7;
    args->seg_size = 20;
//    args->seg_size = 50;
//    args->n_merge = 4;
    args->age_shift = 3;
    args->sample_every_n_seg_for_training = 2;
    args->rank_intvl = 20;
    args->bucket_type = SIZE_BUCKET;
    args->retrain_intvl = 86400;


  } else if (strstr(args->trace_path, "w32") != NULL) {
    uint64_t s[7] = {1000, 2000, 4000, 8000, 10000, 12000, 16000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = 7;

    args->seg_size = 1000;
    args->n_merge = 2;
    args->age_shift = 3;
    args->bucket_type = SIZE_BUCKET;
    args->sample_every_n_seg_for_training = 1;
    args->rank_intvl = 20;
    args->size_bucket_base = 1;
    args->retrain_intvl = 86400;



    args->seg_size = 50;
    args->n_merge = 2;
    args->age_shift = 3;
    args->bucket_type = SIZE_BUCKET;
//    args->min_start_train_seg = 10000;
//    args->max_start_train_seg = 10000;
//    args->n_train_seg_growth = 6000;
    args->sample_every_n_seg_for_training = 1;
    args->rank_intvl = 120;
    args->size_bucket_base = 1;
    args->retrain_intvl = 86400 * 2;


  } else if (strstr(args->trace_path, "w03") != NULL) {
    uint64_t s[8] = {4000, 8000, 12000, 16000, 24000, 32000, 48000, 64000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = 8;
    args->seg_size = 200;
    args->age_shift = 3;
    args->bucket_type = SIZE_BUCKET;
  } else if (strstr(args->trace_path, "w68") != NULL) {
    uint64_t s[9] = {500, 1000, 2000, 4000, 8000, 12000, 16000, 24000, 32000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = 9;
    args->seg_size = 50;
    args->age_shift = 3;
    args->min_start_train_seg = 4000;
    args->max_start_train_seg = 10000;
    args->n_train_seg_growth = 6000;
    args->sample_every_n_seg_for_training = 2;
    args->rank_intvl = 20;
    args->bucket_type = NO_BUCKET;
  } else if (strstr(args->trace_path, "cphy") != NULL) {
    uint64_t s[9] = {500, 1000, 2000, 4000, 8000, 12000, 16000, 24000, 32000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    printf("use cphy default parameter\n");
    args->n_cache_size = 9;
    args->seg_size = 50;
    args->snapshot_intvl = 3600;
    args->age_shift = 3;
    args->bucket_type = SIZE_BUCKET;
  } else if (strstr(args->trace_path, "media_metadata") != NULL) {
    /* media_metadata */
    //    uint64_t cache_sizes[11] = {MiB * 100, MiB * 200, MiB * 300, MiB * 400, MiB * 500, MiB * 600, MiB * 800, MiB * 1000, MiB * 1200, MiB * 1600, MiB * 2000};
    uint64_t s[11] = {100, 200, 300, 400, 500, 600, 800, 1000, 1200, 1600, 2000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = 11;
    args->seg_size = 1000;
    args->age_shift = 1;
//    args->bucket_type = SIZE_BUCKET;
    args->snapshot_intvl = 3600;
    args->retrain_intvl = 3600 * 4;

  } else if (strstr(args->trace_path, "user_activity") != NULL) {
    /* user activity */
    uint64_t s[10] = {200, 500, 1000, 1500, 2000, 3000, 4000, 5000, 6000, 8000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    /* 2GB 8 MQPS 0.0509 on full trace */
    args->n_cache_size = 10;
    args->size_bucket_base = 20;
    args->seg_size = 1000;
    args->age_shift = 1;
//    args->min_start_train_seg = 500;
//    args->sample_every_n_seg_for_training = 1;
//    args->n_train_seg_growth = 2000;
//    args->retrain_intvl = 86400;
    args->bucket_type = SIZE_BUCKET;
    args->rank_intvl = 20;

    /* use LHD for object selection */
    args->snapshot_intvl = 300 * 6;
//    args->snapshot_intvl = 3600;
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
    args->size_bucket_base = 1000;
    args->min_start_train_seg = 10000;
    args->max_start_train_seg = 80000;
    args->n_train_seg_growth = 20000;
    args->sample_every_n_seg_for_training = 4;
    args->retrain_intvl = 86400;
    args->rank_intvl = 20;

  } else if (strstr(args->trace_path, "sjc") != NULL
      || strstr(args->trace_path, "lax") != NULL) {
    /* nyc */
    uint64_t s[14] = {50,   100,  200,  400,  500,  800,  1000,
                      1500, 2000, 3000, 4000, 5000, 6000, 8000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = GiB * s[i];
    }
    args->n_cache_size = 14;
  } else if (strstr(args->trace_path, "cluster52") != NULL) {
    uint64_t s[4] = {128, 256,  512,  1024,};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
  } else if (strstr(args->trace_path, "cluster17") != NULL || strstr(args->trace_path, "cluster19") != NULL) {
    uint64_t s[9] = {512, 1024, 2048, 4096, 8192, 16384};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
  } else if (strstr(args->trace_path, "cluster") != NULL) {
    uint64_t s[9] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
  } else {
    uint64_t s[8] = {10, 50, 100, 400, 1000, 2000, 4000, 8000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = 8;
    args->seg_size = 4000;
    args->bucket_type = NO_BUCKET;
    args->age_shift = 2;

    printf("cannot detect trace name\n");
    //    abort();
  }
#endif
}
