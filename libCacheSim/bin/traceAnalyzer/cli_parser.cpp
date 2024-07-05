

#include <argp.h>
#include <stdbool.h>
#include <string.h>

#include "../../include/libCacheSim/const.h"
#include "../../utils/include/mystr.h"
#include "../../utils/include/mysys.h"
#include "../cli_reader_utils.h"
#include "internal.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *argp_program_version = "traceAnalyzer 0.0.1";
const char *argp_program_bug_address =
    "https://groups.google.com/g/libcachesim/";

enum argp_option_short {
  OPTION_TRACE_TYPE_PARAMS = 't',
  OPTION_OUTPUT_PATH = 'o',
  OPTION_NUM_REQ = 'n',
  OPTION_VERBOSE = 'v',

  OPTION_TIME_WINDOW = 0x100,
  OPTION_WARMUP_SEC = 0x101,
  /* used in access pattern */
  OPTION_ACCESS_PATTERN_SAMPLE_RATIO = 0x102,
  OPTION_TRACK_N_HIT = 0x103,
  OPTION_TRACK_N_POPULAR = 0x104,

  OPTION_ENABLE_ALL = 0x200,
  OPTION_ENABLE_COMMON = 0x201,
  OPTION_ENABLE_POPULARITY = 0x202,
  OPTION_ENABLE_POPULARITY_DECAY = 0x203,
  OPTION_ENABLE_REUSE = 0x204,
  OPTION_ENABLE_SIZE = 0x205,
  OPTION_ENABLE_REQ_RATE = 0x206,
  OPTION_ENABLE_ACCESS_PATTERN = 0x207,
  OPTION_ENABLE_TTL = 0x208,

  // OPTION_ENABLE_LIFETIME = 0x208,
  OPTION_ENABLE_CREATE_FUTURE_REUSE_CCDF = 0x209,
  OPTION_ENABLE_PROB_AT_AGE = 0x20a,
  OPTION_ENABLE_SIZE_CHANGE = 0x20b,
  OPTION_ENABLE_SCAN_DETECTOR = 0x20c,
  OPTION_ENABLE_WRITE_REUSE = 0x20d,
  OPTION_ENABLE_WRITE_FUTURE_REUSE = 0x20e,
  OPTION_ENABLE_WRITE_FUTURE_REUSE_CCDF = 0x20f,
  OPTION_ENABLE_WRITE_REUSE_CCDF = 0x210,
  OPTION_ENABLE_WRITE_REUSE_CCDF2 = 0x211,
  OPTION_ENABLE_WRITE_REUSE_CCDF3 = 0x212,
};

/*
   OPTIONS.  Field 1 in ARGP.
   Order of fields: {NAME, KEY, ARG, FLAGS, DOC}.
*/
static struct argp_option options[] = {
    {NULL, 0, NULL, 0, "trace reader related parameters:", 0},
    {"trace-type-params", OPTION_TRACE_TYPE_PARAMS,
     "time-col=1,obj-id-col=2,obj-size-col=3,delimiter=,", 0,
     "Parameters used for csv trace", 1},
    {"num-req", OPTION_NUM_REQ, "-1", 0,
     "Num of requests to process, default -1 means all requests in the trace",
     1},

    {NULL, 0, NULL, 0, "trace analyzer task options:", 0},
    {"common", OPTION_ENABLE_COMMON, NULL, OPTION_ARG_OPTIONAL,
     "enable common analysis", 2},
    {"all", OPTION_ENABLE_ALL, NULL, OPTION_ARG_OPTIONAL,
     "enable all the analysis", 2},
    {"popularity", OPTION_ENABLE_POPULARITY, NULL, OPTION_ARG_OPTIONAL,
     "enable popularity analysis, output freq:cnt in dataname.popularity file, "
     "and prints the skewness to stdout and stat file",
     3},
    {"popularityDacay", OPTION_ENABLE_POPULARITY_DECAY, NULL,
     OPTION_ARG_OPTIONAL,
     "enable popularity decay analysis, this calculates popularity fade as a "
     "heatmap. It is an expensive analysis, enable only when needed.",
     3},
    {"reuse", OPTION_ENABLE_REUSE, NULL, OPTION_ARG_OPTIONAL,
     "reuse analysis, output a reuse distribution (both real time and virtual "
     "time) in dataname.reuse file",
     3},
    {"size", OPTION_ENABLE_SIZE, NULL, OPTION_ARG_OPTIONAL,
     "size analysis, output a size distribution in dataname.size file", 3},
    {"reqRate", OPTION_ENABLE_REQ_RATE, NULL, OPTION_ARG_OPTIONAL,
     "request rate analysis, requires time-window parameter and output a "
     "request rate distribution in dataname.reqRate file",
     3},
    {"accessPattern", OPTION_ENABLE_ACCESS_PATTERN, NULL, OPTION_ARG_OPTIONAL,
     "access pattern analysis, this is used to visualize the access pattern of "
     "the workload, this is an expensive analysis and often needs manual "
     "tuning the access-pattern-sample-ratio parameter",
     3},
    {"ttl", OPTION_ENABLE_TTL, NULL, OPTION_ARG_OPTIONAL,
     "ttl analysis, output a ttl distribution in dataname.ttl file", 2},

    {NULL, 0, NULL, 0, "trace analyzer related parameters:", 4},
    {"time-window", OPTION_TIME_WINDOW, "300", 0,
     "time window used in per-interval analysis", 4},
    {"warmup-sec", OPTION_WARMUP_SEC, "86400", 0, "warm up before analysis", 4},
    {"access-pattern-sample-ratio", OPTION_ACCESS_PATTERN_SAMPLE_RATIO, "0.01",
     0, "the sampling ratio in access pattern analysis", 4},
    {"track-n-hit", OPTION_TRACK_N_HIT, "8", 0,
     "track one-hit-wonder, two-hit-wonder, etc.", 4},
    {"track-n-popular", OPTION_TRACK_N_POPULAR, "8", 0,
     "track how many requests the n most popular objects get", 4},

    {NULL, 0, NULL, 0, "common parameters:", 0},

    {"output", OPTION_OUTPUT_PATH, "", OPTION_ARG_OPTIONAL, "Output path", 8},
    {"verbose", OPTION_VERBOSE, NULL, OPTION_ARG_OPTIONAL,
     "Produce verbose output", 8},
    {0}};

/*
   PARSER. Field 2 in ARGP.
   Order of parameters: KEY, ARG, STATE.
*/
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments =
      reinterpret_cast<struct arguments *>(state->input);

  switch (key) {
    case OPTION_TRACE_TYPE_PARAMS:
      arguments->trace_type_params = arg;
      break;
    case OPTION_OUTPUT_PATH:
      strncpy(arguments->ofilepath, arg, OFILEPATH_LEN);
      break;
    case OPTION_NUM_REQ:
      arguments->n_req = atoll(arg);
      break;
    case OPTION_TIME_WINDOW:
      arguments->analysis_param.time_window = atoi(arg);
      break;
    case OPTION_WARMUP_SEC:
      arguments->analysis_param.warmup_time = atoi(arg);
      break;
    case OPTION_ACCESS_PATTERN_SAMPLE_RATIO:
      arguments->analysis_param.access_pattern_sample_ratio = atof(arg);
      arguments->analysis_param.access_pattern_sample_ratio_inv =
          (int)(1.0 / arguments->analysis_param.access_pattern_sample_ratio) +
          1;
      break;
    case OPTION_TRACK_N_HIT:
      arguments->analysis_param.track_n_hit = atoi(arg);
      break;
    case OPTION_ENABLE_ALL:
      arguments->analysis_option.req_rate = true;
      arguments->analysis_option.access_pattern = true;
      arguments->analysis_option.size = true;
      arguments->analysis_option.reuse = true;
      arguments->analysis_option.popularity = true;
      arguments->analysis_option.popularity_decay = true;
      break;
    case OPTION_ENABLE_COMMON:
      arguments->analysis_option.req_rate = true;
      arguments->analysis_option.access_pattern = true;
      arguments->analysis_option.size = true;
      arguments->analysis_option.reuse = true;
      arguments->analysis_option.popularity = true;
      break;
    case OPTION_ENABLE_POPULARITY:
      arguments->analysis_option.popularity = true;
      break;
    case OPTION_ENABLE_POPULARITY_DECAY:
      arguments->analysis_option.popularity_decay = true;
      break;
    case OPTION_ENABLE_REUSE:
      arguments->analysis_option.reuse = true;
      break;
    case OPTION_ENABLE_SIZE:
      arguments->analysis_option.size = true;
      break;
    case OPTION_ENABLE_REQ_RATE:
      arguments->analysis_option.req_rate = true;
      break;
    case OPTION_ENABLE_ACCESS_PATTERN:
      arguments->analysis_option.access_pattern = true;
      break;
    case OPTION_ENABLE_TTL:
      arguments->analysis_option.ttl = true;
      break;

    case OPTION_VERBOSE:
      arguments->verbose = is_true(arg) ? true : false;
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
static char args_doc[] = "trace_path trace_type [--task1] [--task2] ...";

/* Program documentation. */
static char doc[] =
    "example: ./bin/traceAnalyzer ../data/trace.vscsi vscsi --common\n\n"
    "trace_type: txt/csv/twr/vscsi/oracleGeneralBin and more\n"
    "if using csv trace, considering specifying -t obj-id-is-num=true\n\n"
    "task: "
    "[common/all/popularity/popularityDecay/reuse/size/reqRate/"
    "accessPattern]\n\n";

/**
 * @brief initialize the arguments
 *
 * @param args
 */
static void init_arg(struct arguments *args) {
  memset(args, 0, sizeof(struct arguments));

  args->trace_path = NULL;
  args->trace_type_params = NULL;
  args->verbose = true;
  memset(args->ofilepath, 0, OFILEPATH_LEN);
  args->n_req = -1;
  args->verbose = false;

  args->analysis_option = traceAnalyzer::default_option();
  args->analysis_param = traceAnalyzer::default_param();

  args->reader = NULL;
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
  const char *trace_type_str = args->args[1];

  if (args->ofilepath[0] == '\0') {
    char *trace_filename = rindex(args->trace_path, '/');
    snprintf(args->ofilepath, OFILEPATH_LEN, "%s",
             trace_filename == NULL ? args->trace_path : trace_filename + 1);
  }

  args->reader = create_reader(trace_type_str, args->trace_path,
                               args->trace_type_params, args->n_req, false, 1);
}

void free_arg(struct arguments *args) { close_reader(args->reader); }

#ifdef __cplusplus
}
#endif