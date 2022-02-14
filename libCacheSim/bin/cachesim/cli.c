//
// Created by Juncheng on 5/24/21.
//

#include "cachesim.h"
#include "priv/params.h"

sim_arg_t parse_cmd(int argc, char *argv[]) {
  if (argc < 4) {
    printf("usage: %s trace_type (twr/vscsi/bin/oracleTwrNS/oracleAkamaiBin/oracleGeneralBin) "
           "data_path cache_size_MiB alg optional: obj_metadata debug L2CacheType seg_size\n",
           argv[0]);
    exit(1);
  }

  sim_arg_t args;
  set_default_arg(&args);

  if (strcasecmp(argv[1], "twr") == 0) {
    args.trace_type = TWR_TRACE;
  } else if (strcasecmp(argv[1], "vscsi") == 0) {
    args.trace_type = VSCSI_TRACE;
  } else if (strcasecmp(argv[1], "bin") == 0) {
    args.trace_type = BIN_TRACE;
  } else if (strcasecmp(argv[1], "oracleTwrNS") == 0
             || strcasecmp(argv[1], "oracleSimTwrNS") == 0) {
    args.trace_type = ORACLE_SIM_TWRNS_TRACE;
  } else if (strcasecmp(argv[1], "oracleGeneral") == 0
             || strcasecmp(argv[1], "oracleGeneralBin") == 0) {
    args.trace_type = ORACLE_GENERAL_TRACE;
  } else if (strcasecmp(argv[1], "oracleAkamaiBin") == 0) {
    args.trace_type = ORACLE_AKAMAI_TRACE;
  } else {
    printf("unknown trace type %s\n", argv[1]);
    abort();
  }

  args.trace_path = argv[2];
  set_param_with_workload(&args);

  args.cache_size = atoi(argv[3]);
  args.alg = argv[4];

  if (argc > 5) args.per_obj_metadata = atoi(argv[5]);
  if (argc > 6) args.debug = atoi(argv[6]);

#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
  if (argc > 7) {
    if (strcasecmp(argv[7], "logOracleLog") == 0) {
      args.L2Cache_type = LOGCACHE_LOG_ORACLE;
    } else if (strcasecmp(argv[7], "logOracleItem") == 0) {
      args.L2Cache_type = LOGCACHE_ITEM_ORACLE;
    } else if (strcasecmp(argv[7], "logOracleBoth") == 0) {
      args.L2Cache_type = LOGCACHE_BOTH_ORACLE;
    } else if (strcasecmp(argv[7], "logLearned") == 0) {
      args.L2Cache_type = LOGCACHE_LEARNED;
    } else if (strcasecmp(argv[7], "segcache") == 0) {
      args.L2Cache_type = SEGCACHE;
    } else {
      printf("support logOracleLog/logOracleItem/logOracleBoth/logLearned/segcache\n");
      abort();
    }
  }

  if (argc > 8) {
    args.seg_size = atoi(argv[8]);
  }
#endif

  reader_t *reader = setup_reader(args.trace_path, args.trace_type, args.obj_id_type, NULL);
  get_num_of_req(reader);

  common_cache_params_t cc_params = {.cache_size = args.cache_size * MiB,
                                     .hashpower = 24,
                                     .default_ttl = 86400 * 300,
                                     .per_obj_overhead = args.per_obj_metadata};
  cache_t *cache;

  if (strcasecmp(args.alg, "lru") == 0) {
    cache = LRU_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "fifo") == 0) {
    cache = FIFO_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "fifomerge") == 0) {
    cache = FIFOMerge_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "lhd") == 0) {
    cache = LHD_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "slru") == 0) {
    SLRU_init_params_t init_params;
    init_params.n_seg = 5;// Currently hard-coded
    cache = SLRU_init(cc_params, &init_params);
  } else if (strcasecmp(args.alg, "sr_lru") == 0) {
    cache = SR_LRU_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "lfu") == 0) {
    cache = LFUFast_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "optimal") == 0) {
    cache = Optimal_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "optimalSize") == 0) {
    cc_params.hashpower -= 4;
    cache = OptimalSize_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "lecar") == 0) {
    cache = LeCaR_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "cacheus") == 0) {
    cache = Cacheus_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "cr_lfu") == 0) {
    cache = CR_LFU_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "lfu") == 0) {
    cache = LFUFast_init(cc_params, NULL);
#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
  } else if (strcasecmp(args.alg, "L2Cache") == 0) {
    L2Cache_init_params_t init_params = {.segment_size = args.seg_size,
                                         .n_merge = args.n_merge,
                                         .type = args.L2Cache_type,
                                         .rank_intvl = args.rank_intvl,
                                         .merge_consecutive_segs = args.merge_consecutive_segs,
                                         .train_source_x = args.train_source_x,
                                         .train_source_y = args.train_source_y,

                                         .hit_density_age_shift = args.age_shift,
                                         .bucket_type = args.bucket_type,
                                         .retrain_intvl = args.retrain_intvl};
    cache = L2Cache_init(cc_params, &init_params);
#endif
  } else {
    printf("do not support %s\n", args.alg);
    abort();
  }

  args.reader = reader;
  args.cache = cache;

  return args;
}
