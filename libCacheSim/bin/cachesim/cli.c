//
// Created by Juncheng on 5/24/21.
//

#include "params/params.h"

sim_arg_t parse_cmd(int argc, char *argv[]) {
  if (argc < 4) {
    // if cache size is specified, then it will only run one simulation at the
    // given size
    printf(
        "usage: %s trace_type data_path alg "
        "(optional: cache_size_in_mb seg_size n_merge rank_intvl bucket_type "
        "train_source)\n\n"
        "param options: \n"
        "trace_type:  "
        "twr/vscsi/bin/oracleTwrNS/oracleAkamaiBin/oracleGeneralBin\n"
        "bucket_type: no_bucket, size_bucket\n"
        "train_source: online, oracle\n"
        "cache_size: if given, one simulation is of given size is ran, this is "
        "used for debugging\n",
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
  } else if (strcasecmp(argv[1], "oracleTwrNS") == 0 ||
             strcasecmp(argv[1], "oracleSimTwrNS") == 0) {
    args.trace_type = ORACLE_SIM_TWRNS_TRACE;
  } else if (strcasecmp(argv[1], "oracleGeneral") == 0 ||
             strcasecmp(argv[1], "oracleGeneralBin") == 0) {
    args.trace_type = ORACLE_GENERAL_TRACE;
  } else if (strcasecmp(argv[1], "oracleGeneralOpNS") == 0) {
    args.trace_type = ORACLE_GENERALOPNS_TRACE;
  } else if (strcasecmp(argv[1], "oracleAkamaiBin") == 0) {
    args.trace_type = ORACLE_AKAMAI_TRACE;
  } else if (strcasecmp(argv[1], "oracleCF1") == 0) {
    args.trace_type = ORACLE_CF1_TRACE;
  } else if (strcasecmp(argv[1], "oracleSysTwrNS") == 0) {
    args.trace_type = ORACLE_SYS_TWRNS_TRACE;
  } else {
    printf("unknown trace type %s\n", argv[1]);
    abort();
  }

  set_param_with_workload(&args, argv[2]);

  args.alg = argv[3];

  if (argc >= 5) {
    int cache_size = atoi(argv[4]);
    if (cache_size > 0) {
      args.cache_size_in_mb = cache_size;
      args.debug = true;
    }
  }

#if defined(ENABLE_GLCache) && ENABLE_GLCache == 1
  if (strncasecmp(args.alg, "GLCache", 7) == 0) {
    if (strcasecmp(args.alg + 8, "oracleLog") == 0) {
      // GLCache-oracleLog
      args.GLCache_type = LOGCACHE_LOG_ORACLE;
    } else if (strcasecmp(args.alg + 8, "oracleItem") == 0) {
      args.GLCache_type = LOGCACHE_ITEM_ORACLE;
    } else if (strcasecmp(args.alg + 8, "oracleBoth") == 0) {
      args.GLCache_type = LOGCACHE_BOTH_ORACLE;
    } else if (strcasecmp(args.alg + 8, "learned") == 0) {
      args.GLCache_type = LOGCACHE_LEARNED;
    } else if (strcasecmp(args.alg + 8, "segcache") == 0) {
      args.GLCache_type = SEGCACHE;
    } else {
      printf("support oracleLog/oracleItem/oracleBoth/learned/segcache\n");
      abort();
    }
    args.alg = "GLCache";
  }

  if (argc >= 6) {
    int seg_size = atoi(argv[5]);
    if (seg_size > 0) {
      args.seg_size = seg_size;
    }
  }

  if (argc >= 7) {
    int n_merge = atoi(argv[6]);
    if (n_merge > 0) {
      args.n_merge = n_merge;
    }
  }

  if (argc >= 8) {
    double rank_intvl = atof(argv[7]);
    if (rank_intvl > 0) {
      args.rank_intvl = rank_intvl;
    }
  }

  if (argc >= 9) {
    ;
  }

  if (argc >= 10) {
    if (strncasecmp(argv[9], "oracle", 6) == 0) {
      args.train_source_y = TRAIN_Y_FROM_ORACLE;
    } else if (strncasecmp(argv[9], "online", 6) == 0) {
      args.train_source_y = TRAIN_Y_FROM_ONLINE;
    } else {
      printf("unknown training source %s\n", argv[9]);
      abort();
    }
  }
#endif

  reader_t *reader =
      setup_reader(args.trace_path, args.trace_type, args.obj_id_type, NULL);

  common_cache_params_t cc_params = {.cache_size = args.cache_size_in_mb * MiB,
                                     .hashpower = 26,
                                     .default_ttl = 86400 * 300,
                                     .per_obj_overhead = args.per_obj_overhead};
  cache_t *cache;

  if (strcasecmp(args.alg, "lru") == 0) {
    cc_params.per_obj_overhead = 8 * 2;
    cache = LRU_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "fifo") == 0) {
    cache = FIFO_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "arc") == 0) {
    cache = ARC_init(cc_params, NULL);
  // } else if (strcasecmp(args.alg, "arc2") == 0) {
  //   cache = ARC2_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "fifomerge") == 0) {
    cc_params.per_obj_overhead = 2;  // freq
    cache = FIFOMerge_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "lhd") == 0) {
    cc_params.per_obj_overhead = 8 * 3 + 1;  // two age, one timestamp, one bool
    cache = LHD_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "slru") == 0) {
    cc_params.per_obj_overhead = 8 * 2;
    cache = SLRU_init(cc_params, "n_seg=4;");
  } else if (strcasecmp(args.alg, "sr_lru") == 0) {
    cache = SR_LRU_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "lfu") == 0) {
    cc_params.per_obj_overhead = 8 * 2;
    cache = LFU_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "belady") == 0) {
    cache = Belady_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "beladySize") == 0) {
    cc_params.hashpower -= 4;
    cache = BeladySize_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "lecar") == 0) {
    cc_params.per_obj_overhead =
        8 * 2 + 8 * 2 + 8;  // LRU chain, LFU chain, history
    cache = LeCaR_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "cacheus") == 0) {
    cc_params.per_obj_overhead =
        8 * 2 + 8 * 2 + 8;  // LRU chain, LFU chain, history
    cache = Cacheus_init(cc_params, NULL);
  } else if (strcasecmp(args.alg, "cr_lfu") == 0) {
    cache = CR_LFU_init(cc_params, NULL);
#if defined(ENABLE_GLCache) && ENABLE_GLCache == 1
  } else if (strcasecmp(args.alg, "GLCache") == 0) {
    cc_params.per_obj_overhead = 2 + 1 + 8;  // freq, bool, history
    // GLCache_init_params_t init_params = {
    //     .segment_size = args.seg_size,
    //     .n_merge = args.n_merge,
    //     .type = args.GLCache_type,
    //     .rank_intvl = args.rank_intvl,
    //     .merge_consecutive_segs = args.merge_consecutive_segs,
    //     .train_source_y = args.train_source_y,
    //     .retrain_intvl = args.retrain_intvl};
    cache = GLCache_init(cc_params, NULL);
#endif
  } else {
    ERROR("do not support algorithm %s\n", args.alg);
    abort();
  }

  args.reader = reader;
  args.cache = cache;

  return args;
}
