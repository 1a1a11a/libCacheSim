

#define _GNU_SOURCE
#include <argp.h>
#include <glib.h>
#include <stdbool.h>
#include <string.h>

#include "../../include/libCacheSim/const.h"
#include "../../utils/include/mysys.h"
#include "../../utils/include/mystr.h"
#include "../cli_utils.h"
#include "internal.h"

const char *argp_program_version = "cachesim 0.0.1";
const char *argp_program_bug_address =
    "https://groups.google.com/g/libcachesim/";

enum argp_option_short {
  OPTION_TRACE_TYPE_PARAMS = 't',
  OPTION_OUTPUT_PATH = 'o',
  OPTION_NUM_REQ = 'n',
  OPTION_VERBOSE = 'v',
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

    {"output", OPTION_OUTPUT_PATH, "output", 0, "Output path", 5},
    {"verbose", OPTION_VERBOSE, "1", 0, "Produce verbose output"},

    {0}};

/*
   PARSER. Field 2 in ARGP.
   Order of parameters: KEY, ARG, STATE.
*/
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  struct arguments *arguments = state->input;

  switch (key) {
    case OPTION_TRACE_TYPE_PARAMS:
      arguments->trace_type_params = arg;
      break;
    case OPTION_OUTPUT_PATH:
      strncpy(arguments->ofilepath, arg, OFILEPATH_LEN);
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
static char args_doc[] = "trace_path trace_type dist_type";

/* Program documentation. */
static char doc[] =
    "example: ./cachesim /trace/path csv stack_dist\n\n"
    "trace can be zstd compressed\n"
    "supported trace_type: txt/csv/twr/vscsi/oracleGeneralBin...\n"
    "supported dist_type: "
    "stack_dist/future_stack_dist/dist_since_last_access/"
    "dist_since_first_access\n";

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
  const char* trace_type_str = args->args[1];
  const char* dist_type_str = args->args[2];

  assert(N_ARGS == 3);

  if (args->ofilepath[0] == '\0') {
    snprintf(args->ofilepath, OFILEPATH_LEN, "%s.dist",
             rindex(args->trace_path, '/') + 1);
  }

  /* convert trace type string to enum */
  args->trace_type =
      trace_type_str_to_enum(trace_type_str, args->trace_path);

  if (strcasecmp(dist_type_str, "stack_dist") == 0) {
    args->dist_type = STACK_DIST;
  } else if (strcasecmp(dist_type_str, "future_stack_dist") == 0) {
    args->dist_type = FUTURE_STACK_DIST;
  } else if (strcasecmp(dist_type_str, "dist_since_last_access") == 0) {
    args->dist_type = DIST_SINCE_LAST_ACCESS;
  } else if (strcasecmp(dist_type_str, "dist_since_first_access") == 0) {
    args->dist_type = DIST_SINCE_FIRST_ACCESS;
  } else {
    ERROR("unsupported dist type %s\n", dist_type_str);
  }

  reader_init_param_t reader_init_params;
  memset(&reader_init_params, 0, sizeof(reader_init_params));
  reader_init_params.ignore_obj_size = true;
  reader_init_params.ignore_size_zero_req = true;
  reader_init_params.obj_id_is_num = true;
  reader_init_params.cap_at_n_req = args->n_req;
  reader_init_params.sampler = NULL;

  parse_reader_params(args->trace_type_params, &reader_init_params);

  if ((args->trace_type == CSV_TRACE || args->trace_type == PLAIN_TXT_TRACE) &&
      reader_init_params.obj_size_field == -1) {
    reader_init_params.ignore_obj_size = true;
  }

  args->reader =
      setup_reader(args->trace_path, args->trace_type, &reader_init_params);
}
