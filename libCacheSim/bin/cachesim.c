//
// Created by Juncheng Yang on 3/31/21.
//

#include "../include/libCacheSim.h"
#include "math.h"


typedef struct {
  char *trace_path;
  int64_t cache_size;
  char *alg;
  trace_type_e trace_type;
  obj_id_type_e obj_id_type;

  int per_obj_metadata;
} sim_arg_t;

sim_arg_t parse_cmd(int argc, char *argv[]) {
  if (argc < 4) {
    printf("usage: %s trace_type (twr/vscsi/bin/oracleBin) data_path cache_size_MiB alg obj_metadata\n", argv[0]);
    exit(1);
  }

  sim_arg_t args = {.obj_id_type = OBJ_ID_NUM};
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
  } else {
    printf("unknown trace type %s\n", argv[1]);
  }

  args.trace_path = argv[2];
  args.cache_size = atoi(argv[3]);
  args.alg = argv[4];
  args.per_obj_metadata = atoi(argv[5]);

  printf("trace type %s, trace %s cache_size %ld MiB alg %s metadata_size %d\n", argv[1], args.trace_path, args.cache_size, args.alg, args.per_obj_metadata);
  return args;
}

double gettime(){
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return tv.tv_sec + tv.tv_usec/1000000.0;
}

void run_cache(reader_t *reader, cache_t *cache) {
  request_t *req = new_request();
  uint64_t req_cnt = 0, miss_cnt = 0;
  uint64_t req_byte = 0, miss_byte = 0;

  read_one_req(reader, req);
  int32_t start_ts = req->real_time, last_report_ts = req->real_time;

  double start_time = gettime();

  while (req->valid) {
    req_cnt++;
    req_byte += req->obj_size;
    if (cache->get(cache, req) != cache_ck_hit) {
      miss_cnt++;
      miss_byte += req->obj_size;
    }

    if (req->real_time - last_report_ts >= 3600 * 24) {
      INFO("ts %lu: %lu requests, miss cnt %lu %.4lf\n",
           (unsigned long) req->real_time - start_ts,
           (unsigned long) req_cnt, (unsigned long) miss_cnt,
           (double) miss_cnt / req_cnt);
      last_report_ts = req->real_time;
    }

    read_one_req(reader, req);
  }

  double runtime = gettime() - start_time;
  INFO("ts %lu: %lu requests, miss cnt %lu %.4lf throughput (MQPS): %.2lf\n",
       (unsigned long) req->real_time - start_ts,
       (unsigned long) req_cnt, (unsigned long) miss_cnt,
       (double) miss_cnt / req_cnt, (double) req_cnt / runtime);
}

int main(int argc, char **argv){

//  reader_init_param_t init_params = {.obj_id_field=6, .has_header=FALSE, .delimiter='\t'};
//  reader_t *reader = setup_reader("../../data/trace.csv", CSV_TRACE, OBJ_ID_STR, &init_params);

  uint64_t cache_sizes[7] = {MiB * 500, MiB * 1000, MiB * 2000, MiB * 4000, MiB * 8000, MiB * 12000, MiB * 16000};
//  uint64_t cache_sizes[2] = {MiB * 2, MiB * 20};
  int n_cache_size = sizeof(cache_sizes) / sizeof(uint64_t);
//  for (int i = 0; i < n_cache_size; i++) {
//    cache_sizes[i] = MiB*100 * (i + 1);
//  }

  sim_arg_t args = parse_cmd(argc, argv);


  reader_t *reader = setup_reader(args.trace_path, args.trace_type, args.obj_id_type, NULL);
  get_num_of_req(reader);

  common_cache_params_t cc_params = {.cache_size=args.cache_size * MiB, .hashpower=(int)log2(reader->n_total_req)-5, .default_ttl=86400*300, .per_obj_overhead=args.per_obj_metadata};
  cache_t *cache;

  if (strcasecmp(args.alg, "lru") == 0)
    cache = LRU_init(cc_params, NULL);
  else if (strcasecmp(args.alg, "optimal") == 0)
    cache = optimal_init(cc_params, NULL);
  else if (strcasecmp(args.alg, "LLSC") == 0) {
    LLSC_init_params_t init_params = {.segment_size=200, .n_merge=2, .type=LOGCACHE_LOG_ORACLE, .rank_intvl=5};
//    LLSC_init_params_t init_params = {.segment_size=200, .n_merge=2, .type=SEGCACHE, .rank_intvl=20};
    cache = LLSC_init(cc_params, &init_params);
  } else {
    printf("do not support %s\n", args.alg);
    abort();
  }

//  run_cache(reader, cache);

  sim_res_t *result = get_miss_ratio_curve(reader, cache, n_cache_size, cache_sizes, NULL, 0, 4);

  for (int i = 0; i < n_cache_size; i++) {
    printf("cache size %"PRIu64": miss/n_req %"PRIu64"/%"PRIu64" (%.4lf)\n",
           result[i].cache_size, result[i].miss_cnt, result[i].req_cnt,
           (double) result[i].miss_cnt/result[i].req_cnt);
  }

  close_reader(reader);
  cache_struct_free(cache);

  return 0;
}

