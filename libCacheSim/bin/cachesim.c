//
// Created by Juncheng Yang on 3/31/21.
//

#include "../include/libCacheSim.h"
#include "math.h"


typedef struct {
  char *trace_path;
  char *alg;
  trace_type_e trace_type;
  obj_id_type_e obj_id_type;
} sim_arg_t;

sim_arg_t parse_cmd(int argc, char *argv[]) {
  if (argc < 4) {
    printf("usage: %s trace_type (twr/vscsi/bin) data_path alg\n", argv[0]);
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
  } else {
    printf("unknown trace type %s\n", argv[1]);
  }

  args.trace_path = argv[2];
  args.alg = argv[3];

  printf("trace type %s, trace %s alg %s\n", argv[1], args.trace_path, args.alg);
  return args;
}

int main(int argc, char **argv){

//  reader_init_param_t init_params = {.obj_id_field=6, .has_header=FALSE, .delimiter='\t'};
//  reader_t *reader = setup_reader("../../data/trace.csv", CSV_TRACE, OBJ_ID_STR, &init_params);
  uint64_t cache_sizes[2] = {MiB * 100, MiB * 1000};
//  uint64_t cache_sizes[4] = {MiB * 10, MiB * 100, MiB * 1000, MiB * 10000};
  int n_cache_size = sizeof(cache_sizes) / sizeof(uint64_t);
//  for (int i = 0; i < n_cache_size; i++) {
//    cache_sizes[i] = MiB*100 * (i + 1);
//  }

  sim_arg_t args = parse_cmd(argc, argv);


  reader_t *reader = setup_reader(args.trace_path, args.trace_type, args.obj_id_type, NULL);
  get_num_of_req(reader);

  request_t* req = new_request();
  read_one_req(reader, req);

  common_cache_params_t cc_params = {.cache_size=20, .hashpower=(int)log2(reader->n_total_req)-2, .default_ttl=86400*300, .per_obj_overhead=24};
  cache_t *cache;

  if (strcasecmp(args.alg, "lru") == 0)
    cache = LRU_init(cc_params, NULL);
  else if (strcasecmp(args.alg, "optimal") == 0)
    cache = optimal_init(cc_params, NULL);
  else {
    printf("do not support %s\n", args.alg);
    abort();
  }

//  cache_t *cache = create_cache(args.alg, cc_params, NULL);


  sim_res_t *result = get_miss_ratio_curve(reader, cache, n_cache_size, cache_sizes, NULL, 0, 1);

  for (int i = 0; i < n_cache_size; i++) {
    printf("cache size %"PRIu64": miss/n_req %"PRIu64"/%"PRIu64" (%.4lf)\n",
           result[i].cache_size, result[i].miss_cnt, result[i].req_cnt,
           (double) result[i].miss_cnt/result[i].req_cnt);
  }

  free_request(req);
  cache_struct_free(cache);

  return 0;
}

