

#define _GNU_SOURCE
#include <argp.h>
#include <glib.h>
#include <stdbool.h>
#include <string.h>

#include "../../include/libCacheSim/const.h"
#include "../../utils/include/mysys.h"
#include "internal.h"

const char *argp_program_version = "cachesim 0.0.1";
const char *argp_program_bug_address = "google group";

enum argp_option_short {
  OPTION_TRACE_TYPE_PARAMS = 't',
  OPTION_EVICTION_PARAMS = 'e',
  OPTION_ADMISSION_ALGO = 'a',
  OPTION_ADMISSION_PARAMS = 0x100,
  OPTION_OUTPUT_PATH = 'o',
  OPTION_N_THREAD = 'n',
  OPTION_IGNORE_OBJ_SIZE = 0x101,
  OPTION_USE_TTL = 0x102,
  OPTION_WARMUP_SEC = 0x104,
  OPTION_VERBOSE = 'v',
  OPTION_CONSIDER_OBJ_METADATA = 0x105,
};

/*
   OPTIONS.  Field 1 in ARGP.
   Order of fields: {NAME, KEY, ARG, FLAGS, DOC}.
*/
static struct argp_option options[] = {
    // {"trace_path", 'i', 0, 0, "The path to the trace file", 0},
    // {"cache_size", 'c', 0, 0,
    //  "Cache size in bytes, set size to 0 to uses default cache size", 1},
    // {"trace_type", 't', 0, 0,
    //  "Specify the type of the input trace:
    //  txt/csv/twr/vscsi/bin/oracleTwrNS/oracleAkamaiBin/oracleGeneralBin", 2},
    {"trace_type_params", OPTION_TRACE_TYPE_PARAMS, "delimiter=,", 0,
     "Parameters used for csv trace, e.g., \"delimiter=,,header=true\".", 2},

    // {"eviction", 'e', 0, 0, "Eviction algorithm: LRU/FIFO/LFU", 3},
    {"eviction_params", OPTION_EVICTION_PARAMS, "n_seg=4", 0,
     "optional params for each eviction algorithm, e.g., n_seg=4", 3},
    {"admission", OPTION_ADMISSION_ALGO, "bloomFilter", 0,
     "Admission algorithm: size/bloomFilter/probabilistic", 4},
    {"admission_params", OPTION_ADMISSION_PARAMS, 0, 0,
     "params for admission algorithm", 4},

    {"output_path", OPTION_OUTPUT_PATH, "output", 0, "Output path", 5},
    {"n_thread", OPTION_N_THREAD, "16", 0,
     "Number of threads if running when using default cache sizes", 5},
    {"verbose", OPTION_VERBOSE, "1", 0, "Produce verbose output"},

    {0, 0, 0, 0, "Other less used options:"},
    {"ignore_obj_size", OPTION_IGNORE_OBJ_SIZE, "false", 0,
     "specify to ignore the object size from the trace", 10},
    {"warmup_sec", OPTION_WARMUP_SEC, "0", 0, "warm up time in seconds", 10},
    {"use_ttl", OPTION_USE_TTL, "false", 0, "specify to use ttl from the trace",
     11},
    {"consider_obj_metadata", OPTION_CONSIDER_OBJ_METADATA, "true", 0,
     "Whether consider per object metadata size in the simulated cache", 10},

    {0}};

static inline bool is_true(const char *arg) {
  if (strcasecmp(arg, "true") == 0 || strcasecmp(arg, "1") == 0 ||
      strcasecmp(arg, "yes") == 0 || strcasecmp(arg, "y") == 0) {
    return true;
  } else if (strcasecmp(arg, "false") == 0 || strcasecmp(arg, "0") == 0 ||
             strcasecmp(arg, "no") == 0 || strcasecmp(arg, "n") == 0) {
    return false;
  } else {
    ERROR("Invalid value: %s, expect true/false", arg);
    abort();
  }
}

/*
   PARSER. Field 2 in ARGP.
   Order of parameters: KEY, ARG, STATE.
*/
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;

  switch (key) {
    case OPTION_N_THREAD:
      arguments->n_thread = atoi(arg);
      if (arguments->n_thread == 0 || arguments->n_thread == -1) {
        arguments->n_thread = n_cores();
      }
      break;
    case OPTION_TRACE_TYPE_PARAMS:
      arguments->trace_type_params = arg;
      break;
    case OPTION_EVICTION_PARAMS:
      arguments->eviction_params = arg;
      break;
    case OPTION_ADMISSION_ALGO:
      arguments->admission_algo = arg;
      break;
    case OPTION_ADMISSION_PARAMS:
      arguments->admission_params = arg;
      break;
    case OPTION_OUTPUT_PATH:
      arguments->ofilepath = arg;
      break;
    case OPTION_VERBOSE:
      arguments->verbose = is_true(arg) ? true : false;
      break;
    case OPTION_USE_TTL:
      arguments->use_ttl = is_true(arg) ? true : false;
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
      if (state->arg_num >= 4) {
        printf("found too many arguments, current %s\n", arg);
        argp_usage(state);
      }
      arguments->args[state->arg_num] = arg;
      break;
    case ARGP_KEY_END:
      if (state->arg_num < 4) {
        printf("not enough arguments found\n");
        argp_usage(state);
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
  args->consider_obj_metadata = true;
  args->n_thread = n_cores();
  args->warmup_sec = 0;
  args->ofilepath = NULL;

  for (int i = 0; i < N_MAX_CACHE_SIZE; i++) {
    args->cache_sizes[i] = 0;
  }
  args->n_cache_size = 0;
}

/**
 * @brief convert cache size string to byte, e.g., 100MB -> 100 * 1024 * 1024
 * the cache size can be an integer or a string with suffix KB/MB/GB
 *
 * @param cache_size_str
 * @return unsigned long
 */
static unsigned long conv_size_str_to_byte_ul(char *cache_size_str) {
  if (strcasecmp(cache_size_str, "auto") == 0) {
    return 0;
  }

  if (strcasestr(cache_size_str, "kb") != NULL ||
      cache_size_str[strlen(cache_size_str) - 1] == 'k' ||
      cache_size_str[strlen(cache_size_str) - 1] == 'K') {
    return strtoul(cache_size_str, NULL, 10) * KiB;
  } else if (strcasestr(cache_size_str, "mb") != NULL ||
             cache_size_str[strlen(cache_size_str) - 1] == 'm' ||
             cache_size_str[strlen(cache_size_str) - 1] == 'M') {
    return strtoul(cache_size_str, NULL, 10) * MiB;
  } else if (strcasestr(cache_size_str, "gb") != NULL ||
             cache_size_str[strlen(cache_size_str) - 1] == 'g' ||
             cache_size_str[strlen(cache_size_str) - 1] == 'G') {
    return strtoul(cache_size_str, NULL, 10) * GiB;
  } else if (strcasestr(cache_size_str, "tb") != NULL ||
             cache_size_str[strlen(cache_size_str) - 1] == 't' ||
             cache_size_str[strlen(cache_size_str) - 1] == 'T') {
    return strtoul(cache_size_str, NULL, 10) * TiB;
  }

  long cache_size = strtol(cache_size_str, NULL, 10);
  cache_size = cache_size == -1 ? 0 : cache_size;

  return (unsigned long)cache_size;
}

/** @brief convert the cache size string to byte,
 * example input1: 1024,2048,4096,8192
 * example input2: 1GB,2GB,4GB,8GB
 * example input3: 1024
 * example input4: 0
 *
 * @param cache_size_str
 * @param args
 * @return int
 */
static int conv_cache_sizes(char *cache_size_str, struct arguments *args) {
  char *token = strtok(cache_size_str, ",");
  args->n_cache_size = 0;
  while (token != NULL) {
    args->cache_sizes[args->n_cache_size++] = conv_size_str_to_byte_ul(token);
    token = strtok(NULL, ",");
  }

  if (args->n_cache_size == 1 && args->cache_sizes[0] == 0) {
    args->n_cache_size = 0;
  }

  return args->n_cache_size;
}

/**
 * @brief convert the trace type string to enum
 *
 * @param args
 */
static void trace_type_str_to_enum(struct arguments *args) {
  if (strcasecmp(args->trace_type_str, "txt") == 0) {
    args->trace_type = PLAIN_TXT_TRACE;
  } else if (strcasecmp(args->trace_type_str, "csv") == 0) {
    args->trace_type = CSV_TRACE;
  } else if (strcasecmp(args->trace_type_str, "twr") == 0) {
    args->trace_type = TWR_TRACE;
  } else if (strcasecmp(args->trace_type_str, "vscsi") == 0) {
    args->trace_type = VSCSI_TRACE;
  } else if (strcasecmp(args->trace_type_str, "oracleGeneralBin") == 0 ||
             strcasecmp(args->trace_type_str, "oracleGeneral") == 0) {
    args->trace_type = ORACLE_GENERAL_TRACE;
  } else if (strcasecmp(args->trace_type_str, "oracleGeneralOpNS") == 0) {
    args->trace_type = ORACLE_GENERALOPNS_TRACE;
  } else if (strcasecmp(args->trace_type_str, "oracleAkamai") == 0) {
    args->trace_type = ORACLE_AKAMAI_TRACE;
  } else if (strcasecmp(args->trace_type_str, "oracleCF1") == 0) {
    args->trace_type = ORACLE_CF1_TRACE;
  } else if (strcasecmp(args->trace_type_str, "oracleSysTwrNS") == 0) {
    args->trace_type = ORACLE_SYS_TWRNS_TRACE;
  } else {
    printf("unsupported trace type: %s\n", args->trace_type_str);
    exit(1);
  }
}

/**
 * @brief print the parsed arguments
 *
 * @param args
 */
static void print_parsed_args(struct arguments *args) {
  char cache_size_str[1024];
  int n = 0;
  for (int i = 0; i < args->n_cache_size; i++) {
    n += snprintf(cache_size_str + n, 1023 - n, "%lu,", args->cache_sizes[i]);
    assert(n < 1024);
  }

  printf(
      "trace path: %s, trace_type %s, cache size %s eviction %s, ofilepath "
      "%s, %d threads, warmup %lu sec",
      args->trace_path, trace_type_str[args->trace_type], cache_size_str,
      args->eviction_algo, args->ofilepath, args->n_thread, args->warmup_sec);
  if (args->trace_type_params != NULL)
    printf(", trace_type_params: %s", args->trace_type_params);
  if (args->admission_algo != NULL)
    printf(", admission: %s", args->admission_algo);
  if (args->admission_params != NULL)
    printf(", admission_params: %s", args->admission_params);
  if (args->eviction_params != NULL)
    printf(", eviction_params: %s", args->eviction_params);
  if (args->use_ttl) printf(", use ttl");
  if (args->ignore_obj_size) printf(", ignore object size");
  if (args->consider_obj_metadata) printf(", consider object metadata");

  printf("\n");
}

/**
 * @brief use the trace file name to verify the trace type (limited use)
 *
 * @param args
 */
static void verify_trace_type(struct arguments *args) {
  if (strcasestr(args->trace_path, "oracleGeneralBin") != NULL ||
      strcasestr(args->trace_path, "oracleGeneral.bin") != NULL) {
    assert(strcasecmp(args->trace_type_str, "oracleGeneral") == 0 ||
           strcasecmp(args->trace_type_str, "oracleGeneralBin") == 0);
  } else if (strcasestr(args->trace_path, "oracleSysTwrNS") != NULL) {
    assert(strcasecmp(args->trace_type_str, "oracleSysTwrNS") == 0);
  } else {
    ;
  }
}

static long cal_working_set_size(reader_t *reader, bool ignore_obj_size) {
  long wss = 0;
  request_t *req = new_request();
  GHashTable *obj_table = g_hash_table_new(g_direct_hash, g_direct_equal);

  INFO("calculating working set size...\n");
  while (read_one_req(reader, req) == 0) {
    if (g_hash_table_contains(obj_table, (gconstpointer)req->obj_id)) {
      continue;
    }

    g_hash_table_add(obj_table, (gpointer)req->obj_id);

    if (ignore_obj_size) {
      wss += 1;
    } else {
      wss += req->obj_size;
    }
  }
  INFO("working set size: %ld %s\n", wss,
       ignore_obj_size ? "objects" : "bytes");

  return wss;
}

static void set_cache_size(struct arguments *args, reader_t *reader) {
#define N_AUTO_CACHE_SIZE 8
  if (args->n_cache_size == 0) {
    if (set_hard_code_cache_size(args)) {
      /* find the hard-coded cache size */
      return;
    }

    // detect cache size from the trace
    long wss = cal_working_set_size(reader, args->ignore_obj_size);
    double s[N_AUTO_CACHE_SIZE] = {0.0001, 0.0003, 0.001, 0.003,
                                   0.01,   0.03,   0.1,   0.3};
    for (int i = 0; i < N_AUTO_CACHE_SIZE; i++) {
      args->cache_sizes[i] = (long)(wss * s[i]);
    }
    args->n_cache_size = N_AUTO_CACHE_SIZE;
  }
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
  // args->cache_size = conv_size_to_byte(args->args[3]);
  assert(N_ARGS == 4);

  if (args->ofilepath == NULL) {
    char *trace_filename = strrchr(args->trace_path, '/');
    args->ofilepath =
        trace_filename == NULL ? args->trace_path : trace_filename + 1;
  }

  /* convert trace type string to enum */
  trace_type_str_to_enum(args);

  /* verify the trace type is correct */
  verify_trace_type(args);

  reader_init_param_t reader_init_params = {
      .ignore_obj_size = args->ignore_obj_size,
      .ignore_size_zero_req = true,
      .obj_id_is_num = true};
  args->reader = setup_reader(args->trace_path, args->trace_type, OBJ_ID_NUM,
                              &reader_init_params);

  set_cache_size(args, args->reader);

  common_cache_params_t cc_params = {
      .cache_size = args->cache_sizes[0],
      .hashpower = 24,
      .default_ttl = 86400 * 300,
      .consider_obj_metadata = args->consider_obj_metadata,
  };
  cache_t *cache;

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
  } else if (strcasecmp(args->eviction_algo, "lfu") == 0) {
    cache = LFU_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "belady") == 0) {
    cache = Belady_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "beladySize") == 0) {
    cc_params.hashpower -= 4;
    cache = BeladySize_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "hyperbolic") == 0) {
    cc_params.hashpower -= 4;
    cache = Hyperbolic_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "lecar") == 0) {
    cache = LeCaR_init(cc_params, args->eviction_params);
  } else if (strcasecmp(args->eviction_algo, "cacheus") == 0) {
    cache = Cacheus_init(cc_params, args->eviction_params);
#if defined(ENABLE_GLCache) && ENABLE_GLCache == 1
  } else if (strcasecmp(args->eviction_algo, "GLCache") == 0) {
    cache = GLCache_init(cc_params, args->eviction_params);
#endif
  } else {
    ERROR("do not support algorithm %s\n", args->eviction_algo);
    abort();
  }

  args->cache = cache;

  print_parsed_args(args);
}
