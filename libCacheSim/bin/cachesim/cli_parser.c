

#define _GNU_SOURCE
#include <argp.h>
#include <glib.h>
#include <stdbool.h>
#include <string.h>

#include "../../include/libCacheSim/const.h"
#include "../../include/libCacheSim/dist.h"
#include "../../include/libCacheSim/prefetchAlgo.h"
#include "../../utils/include/mystr.h"
#include "../../utils/include/mysys.h"
#include "../cli_reader_utils.h"
#include "cache_init.h"
#include "internal.h"

#ifdef __cplusplus
extern "C" {
#endif

static void set_cache_size(struct arguments *args, reader_t *reader);

static int conv_cache_sizes(char *cache_size_str, struct arguments *args);

static void parse_eviction_algo(struct arguments *args, const char *arg);

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
  OPTION_REPORT_INTERVAL = 0x108,

  OPTION_PREFETCH_ALGO = 'p',
  OPTION_PREFETCH_PARAMS = 0x109,
  OPTION_PRINT_HEAD_REQ = 0x10a,
};

/*
   OPTIONS.  Field 1 in ARGP.
   Order of fields: {NAME, KEY, ARG, FLAGS, DOC}.
*/
static struct argp_option options[] = {
    {NULL, 0, NULL, 0, "trace reader related parameters", 0},
    {"trace-type-params", OPTION_TRACE_TYPE_PARAMS,
     "\"obj-id-col=1;delimiter=,\"", 0,
     "Parameters used for csv trace, e.g., \"obj-id-col=1;delimiter=,\"", 2},
    {"num-req", OPTION_NUM_REQ, "-1", 0,
     "Num of requests to process, default -1 means all requests in the trace",
     2},
    {"sample-ratio", OPTION_SAMPLE_RATIO, "1", 0,
     "Sample ratio, 1 means no sampling, 0.01 means sample 1% of objects", 2},

    {NULL, 0, NULL, 0, "cache related parameters:", 0},
    {"eviction-params", OPTION_EVICTION_PARAMS, "\"n-seg=4\"", 0,
     "optional params for each eviction algorithm, e.g., n-seg=4", 4},
    {"admission", OPTION_ADMISSION_ALGO, "bloom-filter", 0,
     "Admission algorithm: size/bloom-filter/prob", 4},
    {"admission-params", OPTION_ADMISSION_PARAMS, "\"prob=0.8\"", 0,
     "params for admission algorithm", 4},
    {"prefetch", OPTION_PREFETCH_ALGO, "Mithril", 0,
     "Prefetching algorithm: Mithril/OBL/PG/AMP", 4},
    {"prefetch-params", OPTION_PREFETCH_PARAMS, "\"block-size=65536\"", 0,
     "optional params for each prefetching algorithm, e.g., block-size=65536",
     4},

    {0, 0, 0, 0, "Other options:"},
    {"ignore-obj-size", OPTION_IGNORE_OBJ_SIZE, "false", 0,
     "specify to ignore the object size from the trace", 6},
    {"output", OPTION_OUTPUT_PATH, "output", 0, "Output path", 6},
    {"num-thread", OPTION_NUM_THREAD, "16", 0,
     "Number of threads if running when using default cache sizes", 6},

    {0, 0, 0, 0, "Other less common options:"},
    {"report-interval", OPTION_REPORT_INTERVAL, "3600", 0,
     "how often to report stat when running one cache", 10},
    {"warmup-sec", OPTION_WARMUP_SEC, "0", 0, "warm up time in seconds", 10},
    {"use-ttl", OPTION_USE_TTL, "false", 0, "specify to use ttl from the trace",
     10},
    {"consider-obj-metadata", OPTION_CONSIDER_OBJ_METADATA, "false", 0,
     "Whether consider per object metadata size in the simulated cache", 10},
    {"verbose", OPTION_VERBOSE, "1", 0, "Produce verbose output", 10},
    {"print-head-req", OPTION_PRINT_HEAD_REQ, "false", 0,
     "Print the first few requests", 10},

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
    case OPTION_PREFETCH_ALGO:
      arguments->prefetch_algo = arg;
      break;
    case OPTION_ADMISSION_PARAMS:
      arguments->admission_params = strdup(arg);
      replace_char(arguments->admission_params, ';', ',');
      replace_char(arguments->admission_params, '_', '-');
      break;
    case OPTION_PREFETCH_PARAMS:
      arguments->prefetch_params = strdup(arg);
      replace_char(arguments->prefetch_params, ';', ',');
      replace_char(arguments->prefetch_params, '_', '-');
      break;
    case OPTION_OUTPUT_PATH:
      strncpy(arguments->ofilepath, arg, OFILEPATH_LEN);
      break;
    case OPTION_NUM_REQ:
      arguments->n_req = atoi(arg);
      break;
    case OPTION_VERBOSE:
      arguments->verbose = is_true(arg) ? true : false;
      break;
    case OPTION_USE_TTL:
      arguments->use_ttl = is_true(arg) ? true : false;
      break;
    case OPTION_REPORT_INTERVAL:
      arguments->report_interval = atol(arg);
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
    case OPTION_PRINT_HEAD_REQ:
      arguments->print_head_req = is_true(arg) ? true : false;
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
    "supported eviction_algo: LRU/LFU/FIFO/ARC/LeCaR/Cacheus\n"
    "print-head-req: Print the first few requests when simulating start\n";

/**
 * @brief initialize the arguments
 *
 * @param args
 */
static void init_arg(struct arguments *args) {
  memset(args, 0, sizeof(struct arguments));

  args->trace_path = NULL;
  args->eviction_params = NULL;
  args->admission_algo = NULL;
  args->prefetch_algo = NULL;
  args->admission_params = NULL;
  args->prefetch_params = NULL;
  args->trace_type_str = NULL;
  args->trace_type_params = NULL;
  args->verbose = true;
  args->use_ttl = false;
  args->ignore_obj_size = false;
  args->consider_obj_metadata = false;
  args->report_interval = 3600 * 24;
  args->n_thread = n_cores();
  args->warmup_sec = -1;
  memset(args->ofilepath, 0, OFILEPATH_LEN);
  args->n_req = -1;
  args->sample_ratio = 1.0;
  args->print_head_req = true;

  for (int i = 0; i < N_MAX_ALGO; i++) {
    args->eviction_algo[i] = NULL;
  }
  args->n_eviction_algo = 0;

  for (int i = 0; i < N_MAX_CACHE_SIZE; i++) {
    args->cache_sizes[i] = 0;
  }
  args->n_cache_size = 0;
}

void free_arg(struct arguments *args) {
  if (args->eviction_params) {
    free(args->eviction_params);
  }
  if (args->admission_params) {
    free(args->admission_params);
  }

  for (int i = 0; i < args->n_eviction_algo; i++) {
    free(args->eviction_algo[i]);
  }

  // free in simulator thread
  // for (int i = 0; i < args->n_eviction_algo * args->n_cache_size; i++) {
  //     args->caches[i]->cache_free(args->caches[i]);
  // }

  close_reader(args->reader);
}

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
  parse_eviction_algo(args, args->args[2]);

  /* the third parameter is the cache size, but we cannot parse it now
   * because we allow user to specify the cache size as fraction of the
   * working set size, and the working set size can only be calculated
   * after we set up the reader */
  assert(N_ARGS == 4);

  if (args->ofilepath[0] == '\0') {
    char *trace_filename = rindex(args->trace_path, '/');
    snprintf(args->ofilepath, OFILEPATH_LEN, "result/%s.cachesim",
             trace_filename == NULL ? args->trace_path : trace_filename + 1);
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

  /** convert the cache sizes from string to int,
   * if the user specifies 0 or auto, we use 12 cache sizes as fraction of
   * the working set size
   * if the user specifies float number, we use the number as the fraction of
   * the working set size **/
  conv_cache_sizes(args->args[3], args);

  for (int i = 0; i < args->n_eviction_algo; i++) {
    for (int j = 0; j < args->n_cache_size; j++) {
      int idx = i * args->n_cache_size + j;
      args->caches[idx] = create_cache(
          args->trace_path, args->eviction_algo[i], args->cache_sizes[j],
          args->eviction_params, args->consider_obj_metadata);

      if (args->admission_algo != NULL) {
        args->caches[idx]->admissioner =
            create_admissioner(args->admission_algo, args->admission_params);
      }

      if (args->prefetch_algo != NULL) {
        args->caches[idx]->prefetcher = create_prefetcher(
            args->prefetch_algo, args->prefetch_params, args->cache_sizes[j]);
      }
    }
  }

  print_parsed_args(args);
}

/**
 * @brief parse the command line eviction_algo arguments
 * the given input is a string, e.g., "LRU,LFU"
 * this function parses the string and stores the parsed eviction algorithms
 * into the args->eviction_algo array
 * and the number of eviction algorithms is stored in args->n_eviction_algo
 */
static void parse_eviction_algo(struct arguments *args, const char *arg) {
#define MAX_ALGO_LEN 4096
  char data[MAX_ALGO_LEN] = {0};
  memcpy(data, arg, strlen(arg));
  char *str = data;
  char *algo = NULL;

  int n_algo = 0;
  while (str != NULL && str[0] != '\0') {
    /* different algorithms are separated by comma */
    algo = strsep(&str, ",");
    args->eviction_algo[n_algo++] = strdup(algo);
  }
  args->n_eviction_algo = n_algo;

#undef MAX_ALGO_LEN
}

/**
 *
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
  long wss = 0;
  args->n_cache_size = 0;
  while (token != NULL) {
    if (strchr(token, '.') != NULL) {
      // input is a float
      if (wss == 0) {
        int64_t wss_obj = 0, wss_byte = 0;
        cal_working_set_size(args->reader, &wss_obj, &wss_byte);
        wss = args->ignore_obj_size ? wss_obj : wss_byte;
      }
      args->cache_sizes[args->n_cache_size++] = (uint64_t)(wss * atof(token));
    } else {
      args->cache_sizes[args->n_cache_size++] = conv_size_str_to_byte_ul(token);
    }

    token = strtok(NULL, ",");
  }

  if (args->n_cache_size == 1 && args->cache_sizes[0] == 0) {
    set_cache_size(args, args->reader);
  }

  return args->n_cache_size;
}

static void set_cache_size(struct arguments *args, reader_t *reader) {
#define N_AUTO_CACHE_SIZE 8
  // if (set_hard_code_cache_size(args)) {
  //   /* find the hard-coded cache size */
  //   return;
  // }

  // detect cache size from the trace
  int n_cache_sizes = 0;
  int64_t wss_obj = 0, wss_byte = 0;
  cal_working_set_size(reader, &wss_obj, &wss_byte);
  int64_t wss = args->ignore_obj_size ? wss_obj : wss_byte;
  double s[N_AUTO_CACHE_SIZE] = {0.001, 0.003, 0.01, 0.03, 0.1, 0.2, 0.4, 0.8};
  for (int i = 0; i < N_AUTO_CACHE_SIZE; i++) {
    if (args->ignore_obj_size) {
      if ((long)(wss_obj * s[i]) > 4) {
        args->cache_sizes[n_cache_sizes++] = (long)(wss * s[i]);
      }
    } else {
      if ((long)(wss_obj * s[i]) >= 100) {
        args->cache_sizes[n_cache_sizes++] = (long)(wss * s[i]);
      }
    }
  }
  args->n_cache_size = n_cache_sizes;

  if (args->n_cache_size == 0) {
    printf("working set %ld too small\n", (long) wss);
    exit(0);
  }
}

/**
 * @brief print the parsed arguments
 *
 * @param args
 */
void print_parsed_args(struct arguments *args) {
#define OUTPUT_STR_LEN 1024
  char output_str[OUTPUT_STR_LEN];
  int n = snprintf(
      output_str, OUTPUT_STR_LEN - 1,
      "trace path: %s, trace_type %s, ofilepath "
      "%s, %d threads, warmup %d sec, total %d algo x %d size = %d caches",
      args->trace_path, g_trace_type_name[args->trace_type], args->ofilepath,
      args->n_thread, args->warmup_sec, args->n_eviction_algo,
      args->n_cache_size, args->n_eviction_algo * args->n_cache_size);

  for (int i = 0; i < args->n_eviction_algo; i++) {
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", %s",
                  args->eviction_algo[i]);
  }

  if (args->trace_type_params != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", trace-type-params: %s", args->trace_type_params);

  if (args->admission_algo != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", admission: %s",
                  args->admission_algo);

  if (args->admission_params != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", admission-params: %s", args->admission_params);

  if (args->prefetch_algo != NULL) {
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", prefetch: %s",
                  args->prefetch_algo);
  }

  if (args->prefetch_params != NULL) {
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", prefetch-params: %s", args->prefetch_params);
  }

  if (args->eviction_params != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", eviction-params: %s", args->eviction_params);

  if (args->use_ttl)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", use ttl");

  if (args->ignore_obj_size)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", ignore object size");

  if (args->consider_obj_metadata)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", consider object metadata");

  snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, "\n");

  INFO("%s", output_str);

#undef OUTPUT_STR_LEN
}

#ifdef __cplusplus
}
#endif
