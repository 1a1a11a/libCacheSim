

#define _GNU_SOURCE
#include <argp.h>
#include <glib.h>
#include <stdbool.h>
#include <string.h>

#include "../../include/libCacheSim/const.h"
#include "../../utils/include/mystr.h"
#include "../../utils/include/mysys.h"
#include "../cli_utils.h"
#include "internal.h"
#include "../../include/libCacheSim/dist.h"


const char *argp_program_version = "cachesim 0.0.1";
const char *argp_program_bug_address =
    "https://groups.google.com/g/libcachesim";

enum argp_option_short {
  OPTION_TRACE_TYPE_PARAMS = 't',
  OPTION_EVICTION_PARAMS = 'e',
  OPTION_ADMISSION_ALGO = 'a',
  OPTION_ADMISSION_PARAMS = 0x100,
  OPTION_OUTPUT_PATH = 'o',
  OPTION_NUM_REQ = 'n',
  OPTION_IGNORE_OBJ_SIZE = 0x101,
  OPTION_USE_TTL = 0x102,
  OPTION_WARMUP_SEC = 0x104,
  OPTION_VERBOSE = 'v',
  OPTION_CONSIDER_OBJ_METADATA = 0x105,
  OPTION_NUM_THREAD = 0x106,
  OPTION_SAMPLE_RATIO = 's',
};

/*
   OPTIONS.  Field 1 in ARGP.
   Order of fields: {NAME, KEY, ARG, FLAGS, DOC}.
*/
static struct argp_option options[] = {
    {"trace-type-params", OPTION_TRACE_TYPE_PARAMS,
     "\"obj-id-col=1;delimiter=,\"", 0,
     "Parameters used for csv trace, e.g., \"obj-id-col=1;delimiter=,\"", 2},
    {"num-req", OPTION_NUM_REQ, "-1", 0,
     "Num of requests to process, default -1 means all requests in the trace"},

    {"eviction-params", OPTION_EVICTION_PARAMS, "\"n-seg=\"4", 0,
     "optional params for each eviction algorithm, e.g., n-seg=4", 3},
    {"admission", OPTION_ADMISSION_ALGO, "bloom-filter", 0,
     "Admission algorithm: size/bloom-filter/prob", 4},
    {"admission-params", OPTION_ADMISSION_PARAMS, "\"prob=0.8\"", 0,
     "params for admission algorithm", 4},
    {"sample-ratio", OPTION_SAMPLE_RATIO, "1", 0,
     "Sample ratio, 1 means no sampling, 0.01 means sample 1% of objects", 5},

    {"output", OPTION_OUTPUT_PATH, "output", 0, "Output path", 5},
    {"num-thread", OPTION_NUM_THREAD, "16", 0,
     "Number of threads if running when using default cache sizes", 5},
    {"verbose", OPTION_VERBOSE, "1", 0, "Produce verbose output"},

    {0, 0, 0, 0, "Other less used options:"},
    {"ignore-obj-size", OPTION_IGNORE_OBJ_SIZE, "false", 0,
     "specify to ignore the object size from the trace", 10},
    {"warmup-sec", OPTION_WARMUP_SEC, "0", 0, "warm up time in seconds", 10},
    {"use-ttl", OPTION_USE_TTL, "false", 0, "specify to use ttl from the trace",
     11},
    {"consider-obj-metadata", OPTION_CONSIDER_OBJ_METADATA, "true", 0,
     "Whether consider per object metadata size in the simulated cache", 10},

    {0}};

/*
   PARSER. Field 2 in ARGP.
   Order of parameters: KEY, ARG, STATE.
*/
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;

  switch (key) {
    case OPTION_NUM_THREAD:
      arguments->n_thread = atoi(arg);
      if (arguments->n_thread == 0 || arguments->n_thread == -1) {
        arguments->n_thread = n_cores();
      }
      break;
    case OPTION_TRACE_TYPE_PARAMS:
      arguments->trace_type_params = arg;
      break;
    case OPTION_EVICTION_PARAMS:
      arguments->eviction_params = strdup(arg);
      replace_char(arguments->eviction_params, ';', ',');
      replace_char(arguments->eviction_params, '_', '-');
      break;
    case OPTION_ADMISSION_ALGO:
      arguments->admission_algo = arg;
      break;
    case OPTION_ADMISSION_PARAMS:
      arguments->admission_params = strdup(arg);
      replace_char(arguments->admission_params, ';', ',');
      replace_char(arguments->admission_params, '_', '-');
      break;
    case OPTION_OUTPUT_PATH:
      strncpy(arguments->ofilepath, arg, OFILEPATH_LEN);
      break;
    case OPTION_VERBOSE:
      arguments->verbose = is_true(arg) ? true : false;
      break;
    case OPTION_USE_TTL:
      arguments->use_ttl = is_true(arg) ? true : false;
      break;
    case OPTION_SAMPLE_RATIO:
      arguments->sample_ratio = atof(arg);
      if (arguments->sample_ratio < 0 || arguments->sample_ratio > 1) {
        ERROR("sample ratio should be in (0, 1]\n");
      }
      break;
    case OPTION_IGNORE_OBJ_SIZE:
      arguments->ignore_obj_size = is_true(arg) ? true : false;
      break;
    case OPTION_CONSIDER_OBJ_METADATA:
      arguments->consider_obj_metadata = is_true(arg) ? true : false;
      break;
    case OPTION_WARMUP_SEC:
      arguments->warmup_sec = atoi(arg);
      break;
    case ARGP_KEY_ARG:
      if (state->arg_num >= N_ARGS) {
        printf("found too many arguments, current %s\n", arg);
        argp_usage(state);
        exit(1);
      }
      arguments->args[state->arg_num] = arg;
      break;
    case ARGP_KEY_END:
      if (state->arg_num < N_ARGS) {
        printf("not enough arguments found\n");
        argp_usage(state);
        exit(1);
      }
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

/*
   ARGS_DOC. Field 3 in ARGP.
   A description of the non-option command-line arguments
     that we accept.
*/
static char args_doc[] = "trace_path trace_type eviction_algo cache_size";

/* Program documentation. */
static char doc[] =
    "example: ./cachesim /trace/path csv LRU 100MB\n\n"
    "trace can be zstd compressed\n"
    "cache_size is in byte, but also support KB/MB/GB\n"
    "supported trace_type: txt/csv/twr/vscsi/oracleGeneralBin\n"
    "supported eviction_algo: LRU/LFU/FIFO/ARC/LeCaR/Cacheus\n";

/**
 * @brief initialize the arguments
 *
 * @param args
 */
static void init_arg(struct arguments *args) {
  memset(args, 0, sizeof(struct arguments));

  args->trace_path = NULL;
  args->eviction_algo = NULL;
  args->eviction_params = NULL;
  args->admission_algo = NULL;
  args->admission_params = NULL;
  args->trace_type_str = NULL;
  args->trace_type_params = NULL;
  args->verbose = true;
  args->use_ttl = false;
  args->ignore_obj_size = false;
  args->consider_obj_metadata = false;
  args->n_thread = n_cores();
  args->warmup_sec = -1;
  memset(args->ofilepath, 0, OFILEPATH_LEN);
  args->n_req = -1;
  args->sample_ratio = 1.0;

  for (int i = 0; i < N_MAX_CACHE_SIZE; i++) {
    args->cache_sizes[i] = 0;
  }
  args->n_cache_size = 0;
}

// void detect_csv_format()

/**
 * @brief parse the command line arguments
 *
 * @param argc
 * @param argv
 */
void parse_cmd(int argc, char *argv[], struct arguments *args) {
  init_arg(args);

  static struct argp argp = {options, parse_opt, args_doc, doc};

  argp_parse(&argp, argc, argv, 0, 0, args);

  args->trace_path = args->args[0];
  args->trace_type_str = args->args[1];
  args->eviction_algo = args->args[2];
  conv_cache_sizes(args->args[3], args);
  assert(N_ARGS == 4);

  if (args->ofilepath[0] == '\0') {
    // char *trace_filename = strrchr(args->trace_path, '/');
    // snprintf(args->ofilepath, OFILEPATH_LEN, "%s.cachesim",
    //          trace_filename == NULL ? args->trace_path : trace_filename + 1);
    snprintf(args->ofilepath, OFILEPATH_LEN, "%s.cachesim", rindex(args->trace_path, '/') + 1);
  }

  /* convert trace type string to enum */
  args->trace_type =
      trace_type_str_to_enum(args->trace_type_str, args->trace_path);

  reader_init_param_t reader_init_params;
  memset(&reader_init_params, 0, sizeof(reader_init_params));
  reader_init_params.ignore_obj_size = args->ignore_obj_size;
  reader_init_params.ignore_size_zero_req = true;
  reader_init_params.obj_id_is_num = true;
  reader_init_params.cap_at_n_req = args->n_req;
  reader_init_params.sampler = NULL;

  parse_reader_params(args->trace_type_params, &reader_init_params);

  if (args->sample_ratio > 0 && args->sample_ratio < 1 - 1e-6) {
    sampler_t *sampler = create_spatial_sampler(args->sample_ratio);
    reader_init_params.sampler = sampler;
  }

  if ((args->trace_type == CSV_TRACE || args->trace_type == PLAIN_TXT_TRACE) &&
      reader_init_params.obj_size_field == -1) {
    args->consider_obj_metadata = false;
    args->ignore_obj_size = true;
    reader_init_params.ignore_obj_size = true;
  }

  args->reader =
      setup_reader(args->trace_path, args->trace_type, &reader_init_params);

  if (args->consider_obj_metadata &&
      should_disable_obj_metadata(args->reader)) {
    INFO("disable object metadata\n");
    args->consider_obj_metadata = false;
  }

  set_cache_size(args, args->reader);

  common_cache_params_t cc_params = {
      .cache_size = args->cache_sizes[0],
      .hashpower = 24,
      .default_ttl = 86400 * 300,
      .consider_obj_metadata = args->consider_obj_metadata,
  };
  cache_t *cache;

  /* the trace provided is small */
  if (strcasestr(args->trace_path, "data/trace.") != NULL)
    cc_params.hashpower -= 8;

  if (strcasecmp(args->eviction_algo, "lru") == 0) {
    cache = LRU_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "fifo") == 0) {
    cache = FIFO_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "arc") == 0) {
    cache = ARC_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "fifomerge") == 0 ||
             strcasecmp(args->eviction_algo, "fifo-merge") == 0) {
    cache = FIFO_Merge_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "fifo-reinsertion") == 0) {
    cache = FIFO_Reinsertion_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "lhd") == 0) {
    cache = LHD_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "lfu") == 0) {
    cache = LFU_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "gdsf") == 0) {
    cache = GDSF_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "lfuda") == 0) {
    cache = LFUDA_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "slru") == 0) {
    cache = SLRU_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "sfifo") == 0) {
    cache = SFIFO_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "lfu") == 0) {
    cache = LFU_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "hyperbolic") == 0) {
    cc_params.hashpower -= 4;
    cache = Hyperbolic_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "lecar") == 0) {
    cache = LeCaR_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "cacheus") == 0) {
    cache = Cacheus_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "lfucpp") == 0) {
    cache = LFUCpp_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "belady") == 0) {
    cache = Belady_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "lru-belady") == 0) {
    if (strstr(args->trace_path, ".zst") != NULL) {
      ERROR("lru-belady only supports uncompressed trace files\n");
    }
    reader_t *reader = clone_reader(args->reader);
    cache = LRU_Belady_init(cc_params, args->eviction_params);
    cache->future_stack_dist = get_stack_dist(
        reader, FUTURE_STACK_DIST, &(cache->future_stack_dist_array_size));
    assert(get_num_of_req(reader) == cache->future_stack_dist_array_size); 
    close_reader(reader);

  } else if (strcasecmp(args->eviction_algo, "sfifo-belady") == 0) {
    if (strstr(args->trace_path, ".zst") != NULL) {
      ERROR("sfifo-belady only supports uncompressed trace files\n");
    }
    reader_t *reader = clone_reader(args->reader);
    cache = SFIFO_Belady_init(cc_params, args->eviction_params);
    cache->future_stack_dist = get_stack_dist(
        reader, FUTURE_STACK_DIST, &(cache->future_stack_dist_array_size));
    assert(get_num_of_req(reader) == cache->future_stack_dist_array_size);
    close_reader(reader);

  } else if (strcasecmp(args->eviction_algo, "beladySize") == 0) {
    cc_params.hashpower -= 4;
    cache = BeladySize_init(cc_params, args->eviction_params);
#if defined(ENABLE_GLCache) && ENABLE_GLCache == 1
  } else if (strcasecmp(args->eviction_algo, "GLCache") == 0) {
    cache = GLCache_init(cc_params, args->eviction_params);
#endif
  } else {
    ERROR("do not support algorithm %s\n", args->eviction_algo);
    abort();
  }

  args->cache = cache;
  if (args->admission_algo != NULL) {
    admissioner_t *admissioner =
        create_admissioner(args->admission_algo, args->admission_params);
    args->cache->admissioner = admissioner;
  }

  print_parsed_args(args);
}
