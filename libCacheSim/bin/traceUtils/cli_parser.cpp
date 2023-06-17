
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <argp.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "../../include/libCacheSim/const.h"
#include "../../utils/include/mysys.h"
#include "../cli_reader_utils.h"
#include "internal.hpp"

namespace cli {
const char *argp_program_version = "traceUtil 0.0.1";
const char *argp_program_bug_address =
    "https://groups.google.com/g/libcachesim";

enum argp_option_short {
  OPTION_TRACE_TYPE_PARAMS = 't',
  OPTION_OUTPUT_PATH = 'o',
  OPTION_SAMPLE_RATIO = 's',
  OPTION_IGNORE_OBJ_SIZE = 0x101,

  // trace conv
  OPTION_OUTPUT_TXT = 0x102,
  OPTION_REMOVE_SIZE_CHANGE = 0x103,

  // trace print
  OPTION_NUM_REQ = 'n',

  // trace filter
  OPTION_FILTER_TYPE = 0x301,
  OPTION_FILTER_SIZE = 0x302
};

/*
   OPTIONS.  Field 1 in ARGP.
   Order of fields: {NAME, KEY, ARG, FLAGS, DOC}.
*/
static struct argp_option options[] = {
    {0, 0, 0, 0, "Options used by all the utilities:"},
    {"trace-type-params", OPTION_TRACE_TYPE_PARAMS,
     "\"obj-id-col=1,header=true\"", 0,
     "Parameters used for csv trace, e.g., \"obj-id-col=1,header=true\"", 2},
    {"output", OPTION_OUTPUT_PATH, "/path/output", 0, "Path to write output",
     2},
    {"sample-ratio", OPTION_SAMPLE_RATIO, "1", 0,
     "Sample ratio, 1 means no sampling, 0.01 means sample 1% of objects", 2},
    {"ignore-obj-size", OPTION_IGNORE_OBJ_SIZE, "false", 0,
     "specify to ignore the object size from the trace", 2},

    {0, 0, 0, 0, "traceConv options:"},
    {"output-txt", OPTION_OUTPUT_TXT, "false", 0,
     "output trace in txt format in addition to binary format", 4},
    {"remove-size-change", OPTION_REMOVE_SIZE_CHANGE, "false", 0,
     "whether remove object size change, if true, objects with changed size "
     "are updated to the old size",
     4},

    {0, 0, 0, 0, "tracePrint options:"},
    {"num-req", OPTION_NUM_REQ, "-1", 0,
     "Number of requests to process, -1 means all requests in the trace", 6},

    {0, 0, 0, 0, "traceFilter options:"},
    {"filter-type", OPTION_FILTER_TYPE, "FIFO", 0,
     "The filter type, e.g., FIFO and LRU", 8},
    {"filter-size", OPTION_FILTER_SIZE, "0.1", 0,
     "The size of the filter, can be absolute size or relative to working set",
     8},

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
    case OPTION_IGNORE_OBJ_SIZE:
      arguments->ignore_obj_size = is_true(arg) ? true : false;
      break;
    case OPTION_OUTPUT_PATH:
      strncpy(arguments->ofilepath, arg, OFILEPATH_LEN - 1);
      break;
    case OPTION_SAMPLE_RATIO:
      arguments->sample_ratio = atof(arg);
      if (arguments->sample_ratio < 0 || arguments->sample_ratio > 1) {
        ERROR("sample ratio should be in (0, 1]\n");
      }
      break;
    case OPTION_REMOVE_SIZE_CHANGE:
      arguments->remove_size_change = is_true(arg) ? true : false;
      break;
    case OPTION_OUTPUT_TXT:
      arguments->output_txt = is_true(arg) ? true : false;
      break;
    case OPTION_NUM_REQ:
      arguments->n_req = atoll(arg);
      break;
    case OPTION_FILTER_TYPE:
      arguments->cache_name = arg;
      break;
    case OPTION_FILTER_SIZE:
      arguments->cache_size = atof(arg);
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
static char args_doc[] = "trace_path trace_type";

/* Program documentation. */
static char doc[] =
    "\n"
    "tracePrint: utility to print binary trace in human-readable format\n"
    "traceConv: utility to convert a trace to oracleGeneral format\n\n"
    "traceFilter: utility to filter a trace\n\n"
    "example usage: ./traceConv /trace/path csv -o "
    "/path/new_trace.oracleGeneral -t "
    "\"obj-id-col=5,time-col=2,obj-size-col=4\"\n\n"
    "example usage: ./traceFilter /trace/path lcs -o /path/new_trace.lcs "
    "--filter fifo --filter-size 0.1\n\n";

/**
 * @brief initialize the arguments
 *
 * @param args
 */
static void init_arg(struct arguments *args) {
  memset(args, 0, sizeof(struct arguments));

  args->n_req = -1;
  args->trace_path = NULL;
  args->trace_type_str = NULL;
  args->trace_type_params = NULL;
  args->ignore_obj_size = false;
  args->sample_ratio = 1.0;
  memset(args->ofilepath, 0, OFILEPATH_LEN);
  args->output_txt = false;
  args->remove_size_change = false;
  args->cache_name = NULL;
  args->cache_size = 0;
}

static void print_parsed_arg(struct arguments *args) {
#define OUTPUT_STR_LEN 1024
  int n = 0;
  char output_str[OUTPUT_STR_LEN];
  n = snprintf(output_str, OUTPUT_STR_LEN - 1, "trace path: %s, trace_type %s",
               args->trace_path, trace_type_str[args->trace_type]);

  if (args->trace_type_params != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", trace type params: %s", args->trace_type_params);

  if (args->sample_ratio < 1.0)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", sample ratio: %lf",
                  args->sample_ratio);

  if (args->n_req != -1)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", num requests to process: %ld", (long)args->n_req);

  if (args->output_txt)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", output txt trace: true");

  if (args->remove_size_change)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", remove size change during traceConv");

  if (args->ignore_obj_size)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", ignore object size");

  snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, "\n");

  INFO("%s", output_str);

#undef OUTPUT_STR_LEN
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
  assert(N_ARGS == 2);

  args->reader = create_reader(args->trace_type_str, args->trace_path,
                               args->trace_type_params, args->n_req, args->ignore_obj_size, 0);

  print_parsed_arg(args);
}

void free_arg(struct arguments *args) { close_reader(args->reader); }
}  // namespace cli
