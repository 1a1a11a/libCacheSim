//
// Created by Juncheng Yang on 3/31/21.
//

#include "../include/libCacheSim.h"
#include "math.h"
#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <assert.h>

typedef struct {
  char *trace_path;
  char *alg;
  trace_type_e trace_type;
  obj_id_type_e obj_id_type;
  int per_obj_metadata;

  int64_t cache_size;
  struct {
    uint64_t cache_sizes[128];
    int n_cache_size;
  };

  int n_thread;
  bool debug;

  struct {
    LSC_type_e lsc_type;
    bucket_type_e bucket_type;
    int seg_size;
    int n_merge;
    int rank_intvl;
    int size_bucket_base;
    int age_shift;
    int min_start_train_seg;
    int max_start_train_seg;
    int n_train_seg_growth;
    int re_train_intvl;
    int sample_every_n_seg_for_training;
  };
} sim_arg_t;

unsigned int n_cores() {
  unsigned int eax = 11, ebx = 0, ecx = 1, edx = 0;

  asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "0"(eax), "2"(ecx) :);

  //  printf("Cores: %d\nThreads: %d\nActual thread: %d\n", eax, ebx, edx);
  return ebx;
}

static void set_default_arg(sim_arg_t *args) {

  args->obj_id_type = OBJ_ID_NUM;

  args->seg_size = 1000;
  args->n_merge = 2;
  args->rank_intvl = 20;
  args->age_shift = 0;
  args->min_start_train_seg = 1000;
  args->max_start_train_seg = 10000;
  args->n_train_seg_growth = 1000;
  args->sample_every_n_seg_for_training = 1;
  args->re_train_intvl = 86400;

  args->bucket_type = NO_BUCKET;
  args->size_bucket_base = 1;
  args->n_thread = 4;

#ifdef __linux__
  //  printf("This system has %d processors configured and "
  //         "%d processors available.\n",
  //         get_nprocs_conf(), get_nprocs());
  args->n_thread = get_nprocs();
#endif

  args->n_thread = (int) n_cores();
}

static void set_param_with_workload(sim_arg_t *args) {
  if (strstr(args->trace_path, "w105") != NULL) {
    /* w105 */
    uint64_t s[7] = {1000, 2000, 4000, 8000, 10000, 12000, 16000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = 7;
    args->seg_size = 20;
    //    args->seg_size = 50;
    args->age_shift = 3;
    args->min_start_train_seg = 500;


    args->min_start_train_seg = 10000;
        args->max_start_train_seg = 10000;
        args->n_train_seg_growth = 10000;
        args->sample_every_n_seg_for_training = 2;
        args->rank_intvl = 20;

    args->bucket_type = SIZE_BUCKET;
  } else if (strstr(args->trace_path, "w32") != NULL) {
    uint64_t s[7] = {1000, 2000, 4000, 8000, 10000, 12000, 16000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = 7;
    printf("found you!\n");

    args->seg_size = 1000;
    args->n_merge = 2;
    args->age_shift = 3;
    args->bucket_type = SIZE_BUCKET;
    args->min_start_train_seg = 4000;
    args->max_start_train_seg = 10000;
    args->n_train_seg_growth = 6000;
    args->sample_every_n_seg_for_training = 1;
    args->rank_intvl = 20;
    args->size_bucket_base = 1;
    args->re_train_intvl = 86400;


    /* 8GB 0.1712 0.66 MQPS */
//    args->seg_size = 20;
//    args->age_shift = 3;
//    args->bucket_type = SIZE_BUCKET;
//    args->min_start_train_seg = 4000;
//    args->max_start_train_seg = 10000;
//    args->n_train_seg_growth = 6000;
//    args->sample_every_n_seg_for_training = 2;
//    args->rank_intvl = 20;


    /* 8GB 0.1736 using 50% trace as warmup 1.42 MQPS */
    /* new 8GB 0.1714 using 50% trace as warmup 1.5 MQPS */
//    args->seg_size = 50;
//    args->age_shift = 3;
//    args->bucket_type = SIZE_BUCKET;
//    args->min_start_train_seg = 4000;
//    args->max_start_train_seg = 10000;
//    args->n_train_seg_growth = 6000;
//    args->sample_every_n_seg_for_training = 1;
//    args->rank_intvl = 20;
//    args->size_bucket_base = 1;

    /* 8GB 0.1865 3 MQPS */
    /* 8GB 0.1709 3.4 MQPS sample_n_seg_train is 2 */
    /* 8GB 0.1747 3.4 MQPS sample_n_seg_train is 1 */
//    args->seg_size = 200;
//    args->age_shift = 3;
//    args->bucket_type = SIZE_BUCKET;
//    args->min_start_train_seg = 4000;
//    args->max_start_train_seg = 10000;
//    args->n_train_seg_growth = 6000;
//    args->sample_every_n_seg_for_training = 1;
//    args->rank_intvl = 20;

    /* 8GB 0.1964 4 MQPS */
    /* 8GB 0.1990 5.6 MQPS */
//    args->seg_size = 1000;
//    args->age_shift = 3;
//    args->bucket_type = SIZE_BUCKET;
//    args->min_start_train_seg = 4000;
//    args->max_start_train_seg = 10000;
//    args->n_train_seg_growth = 6000;
//    args->sample_every_n_seg_for_training = 1;
//    args->rank_intvl = 20;

    /* 8GB 0.2173 6.1 MQPS */
//    args->seg_size = 1500;
//    args->age_shift = 3;
//    args->bucket_type = SIZE_BUCKET;
//    args->min_start_train_seg = 4000;
//    args->max_start_train_seg = 10000;
//    args->n_train_seg_growth = 6000;
//    args->sample_every_n_seg_for_training = 1;
//    args->rank_intvl = 20;

    /* 8GB 0.2456 6.3 MQPS */
//    args->seg_size = 2000;
//    args->age_shift = 3;
//    args->bucket_type = SIZE_BUCKET;
//    args->min_start_train_seg = 4000;
//    args->max_start_train_seg = 10000;
//    args->n_train_seg_growth = 6000;
//    args->sample_every_n_seg_for_training = 1;
//    args->rank_intvl = 20;
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
    args->age_shift = 3;

    // new
    args->min_start_train_seg = 10000;
    args->max_start_train_seg = 80000;
    args->n_train_seg_growth = 10000;
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
    args->min_start_train_seg = 500;
    args->sample_every_n_seg_for_training = 1;
    args->n_train_seg_growth = 2000;
    args->re_train_intvl = 86400;
    args->bucket_type = SIZE_BUCKET;
    args->rank_intvl = 20;


    args->min_start_train_seg = 2500;
    args->sample_every_n_seg_for_training = 1;
    args->n_train_seg_growth = 5000;
    args->max_start_train_seg = 2500;
//    args->re_train_intvl = 86400;
    args->re_train_intvl = 7200;
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
    args->re_train_intvl = 86400;
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
  } else {
    uint64_t s[8] = {10, 50, 100, 400, 1000, 2000, 4000, 8000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = 8;
    args->seg_size = 4000;
    args->n_thread = 4;
    args->bucket_type = NO_BUCKET;
    args->age_shift = 2;

    printf("cannot detect trace name\n");
    //    abort();
  }
}

sim_arg_t parse_cmd(int argc, char *argv[]) {
  if (argc < 4) {
    printf("usage: %s trace_type (twr/vscsi/bin/oracleBin) "
           "data_path cache_size_MiB alg obj_metadata debug LLSC seg_size\n",
           argv[0]);
    exit(1);
  }

  sim_arg_t args;
  set_default_arg(&args);

  if (strcmp(argv[1], "twr") == 0) {
    args.trace_type = TWR_TRACE;
  } else if (strcmp(argv[1], "vscsi") == 0) {
    args.trace_type = VSCSI_TRACE;
  } else if (strcmp(argv[1], "bin") == 0) {
    args.trace_type = BIN_TRACE;
  } else if (strcmp(argv[1], "oracleTwr") == 0) {
    args.trace_type = ORACLE_TWR_TRACE;
  } else if (strcmp(argv[1], "oracleBin") == 0) {
    args.trace_type = ORACLE_BIN_TRACE;
  } else if (strcmp(argv[1], "oracleAkamaiBin") == 0) {
    args.trace_type = ORACLE_AKAMAI_TRACE;
  } else {
    printf("unknown trace type %s\n", argv[1]);
  }

  args.trace_path = argv[2];
  set_param_with_workload(&args);

  args.cache_size = atoi(argv[3]);
  args.alg = argv[4];
  args.per_obj_metadata = atoi(argv[5]);
  args.debug = atoi(argv[6]);

  if (argc > 7) {
    if (strcasecmp(argv[7], "logOracleLog") == 0) {
      args.lsc_type = LOGCACHE_LOG_ORACLE;
    } else if (strcasecmp(argv[7], "logOracleBoth") == 0) {
      args.lsc_type = LOGCACHE_BOTH_ORACLE;
    } else if (strcasecmp(argv[7], "logLearned") == 0) {
      args.lsc_type = LOGCACHE_LEARNED;
    } else if (strcasecmp(argv[7], "segcache") == 0) {
      args.lsc_type = SEGCACHE;
    } else {
      printf("support logOracleLog/logOracleBoth/logLearned/segcache\n");
      abort();
    }
  }

  if (argc > 8) {
    args.seg_size = atoi(argv[8]);
  }

  //  args.bucket_type = NO_BUCKET;

  return args;
}

double gettime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void run_cache(reader_t *reader, cache_t *cache) {
  request_t *req = new_request();
  uint64_t req_cnt = 0, miss_cnt = 0;
  uint64_t req_byte = 0, miss_byte = 0;

  read_one_req(reader, req);
  //  if (strstr(reader->trace_path, "sjc") != NULL) {
  //    for (int i = 0; i < 200000; i++)
  //      read_one_req(reader, req);
  //  }
  int32_t start_ts = req->real_time, last_report_ts = 0;

  /* skip half of the requests */
  int64_t n_skipped = 0;
//  for (int i = 0; i < reader->n_total_req / 2; i++) {
//    n_skipped += 1;
//    req->real_time -= start_ts;
//    cache->get(cache, req);
//    read_one_req(reader, req);
//  }

  double start_time = gettime();
  while (req->valid) {
    req->real_time -= start_ts;

    req_cnt++;
    req_byte += req->obj_size;
    if (cache->get(cache, req) != cache_ck_hit) {
      miss_cnt++;
      miss_byte += req->obj_size;
    }

    if (req->real_time - last_report_ts >= 3600 * 24 && req->real_time != 0) {
      INFO("ts %lu: %lu requests, miss cnt %lu %.4lf, byte miss ratio %.4lf\n",
           (unsigned long) req->real_time, (unsigned long) req_cnt, (unsigned long) miss_cnt,
           (double) miss_cnt / req_cnt, (double) miss_byte / req_byte);
#ifdef TRACK_EVICTION_AGE
      print_eviction_age(cache);
#endif
      last_report_ts = req->real_time;
    }

    read_one_req(reader, req);
  }

  double runtime = gettime() - start_time;
  printf("runtime %lf s\n", runtime);
  INFO("ts %lu: %lu requests, miss cnt %lu %.4lf throughput (MQPS): %.2lf, skipped %ld "
       "requests\n",
       (unsigned long) req->real_time, (unsigned long) req_cnt, (unsigned long) miss_cnt,
       (double) miss_cnt / req_cnt, (double) req_cnt / 1000000.0 / runtime,
       n_skipped);
}

int main(int argc, char **argv) {

  //  reader_init_param_t init_params = {.obj_id_field=6, .has_header=FALSE, .delimiter='\t'};
  //  reader_t *reader = setup_reader("../../data/trace.csv", CSV_TRACE, OBJ_ID_STR, &init_params);

  sim_arg_t args = parse_cmd(argc, argv);

  reader_t *reader = setup_reader(args.trace_path, args.trace_type, args.obj_id_type, NULL);
  get_num_of_req(reader);

  common_cache_params_t cc_params = {.cache_size = args.cache_size * MiB,
                                     .hashpower = 26,
                                     .default_ttl = 86400 * 300,
                                     .per_obj_overhead = args.per_obj_metadata};
  cache_t *cache;

  if (strcasecmp(args.alg, "lru") == 0) cache = LRU_init(cc_params, NULL);
  else if (strcasecmp(args.alg, "fifo") == 0) cache = FIFO_init(cc_params, NULL);
  else if (strcasecmp(args.alg, "optimal") == 0)
    cache = optimal_init(cc_params, NULL);
  else if (strcasecmp(args.alg, "LLSC") == 0) {
    LLSC_init_params_t init_params = {.segment_size = args.seg_size,
                                      .n_merge = args.n_merge,
                                      .type = args.lsc_type,
                                      .rank_intvl = args.rank_intvl,
                                      .hit_density_age_shift = args.age_shift,
                                      .bucket_type = args.bucket_type,
                                      .size_bucket_base = args.size_bucket_base,
                                      .min_start_train_seg = args.min_start_train_seg,
                                      .max_start_train_seg = args.max_start_train_seg,
                                      .n_train_seg_growth = args.n_train_seg_growth,
                                      .re_train_intvl = args.re_train_intvl,
                                      .sample_every_n_seg_for_training =
                                          args.sample_every_n_seg_for_training};
    cache = LLSC_init(cc_params, &init_params);
  } else {
    printf("do not support %s\n", args.alg);
    abort();
  }

  if (args.debug) {
    printf("trace type %s, trace %s cache_size %ld MiB alg %s metadata_size %d, "
           "seg size %d, n merge %d, rank_intvl %d, bucket type %d\n",
           argv[1], args.trace_path, (long) args.cache_size, args.alg, args.per_obj_metadata,
           args.seg_size, args.n_merge, args.rank_intvl, args.bucket_type);

    run_cache(reader, cache);
  } else {
    sim_res_t *result = get_miss_ratio_curve(reader, cache, args.n_cache_size, args.cache_sizes,
                                             NULL, 0, args.n_thread);

    for (int i = 0; i < args.n_cache_size; i++) {
      printf("cache size %16" PRIu64 ": miss/n_req %16" PRIu64 "/%16" PRIu64 " (%.4lf), "
             "byte miss ratio %.4lf\n",
             result[i].cache_size, result[i].miss_cnt, result[i].req_cnt,
             (double) result[i].miss_cnt / result[i].req_cnt,
             (double) result[i].miss_bytes / result[i].req_bytes);
    }
  }

  close_reader(reader);
  cache_struct_free(cache);

  return 0;
}
