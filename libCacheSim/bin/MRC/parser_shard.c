#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../traceReader/sampling/SHARD.c"
#include "../cli_reader_utils.h"
#include "mrc_internal.h"

enum argp_option_short {
  // Removed explicit ver, rate, and trace-type options
  OPTION_SIZE = 's',
  OPTION_TRACE_TYPE_PARAMS = 'P',
  OPTION_NUM_REQ = 'n',
  OPTION_IGNORE_OBJ_SIZE = 'i',
};

static struct argp_option options[] = {
    {"size", OPTION_SIZE, "SIZE", 0, "Size for fixed-size mode (implies fixed-size mode)"},
    {"trace-type-params", OPTION_TRACE_TYPE_PARAMS, "\"obj-id-col=1;delimiter=,\"", 0,
     "Parameters used for csv trace, e.g., \"obj-id-col=1;delimiter=,\""},
    {"num-req", OPTION_NUM_REQ, "REQ", 0, "Num of requests to process; default -1 means all requests"},
    {"ignore-obj-size", OPTION_IGNORE_OBJ_SIZE, "false", 0, "specify to ignore the object size from the trace"},
    {0}
};


static char args_doc[] = "OUTPUT_FILE TRACE_FILE TRACE_TYPE RATE";
static char doc[] =
    "Command line options for SHARDS MRC. The trace file, trace type, and sampling rate "
    "must be provided as positional arguments. If the optional --size is provided, fixed-size mode is used.";

static error_t parse_opt (int key, char *arg, struct argp_state *state) {
  struct SHARD_arguments *arguments = state->input;
  switch (key) {
    case OPTION_SIZE:
      arguments->size = atol(arg);
      // When --size is provided, automatically set fixed-size mode.
      arguments->verver = true;
      break;
    case OPTION_TRACE_TYPE_PARAMS:
      arguments->trace_type_params = arg;
      break;
    case OPTION_IGNORE_OBJ_SIZE:
      arguments->ignore_obj_size = is_true(arg) ? true : false;
      break;
    case OPTION_NUM_REQ:
      arguments->n_req = atoi(arg);
      break;
    case ARGP_KEY_ARG:
      if (state->arg_num == 2) {
        arguments->trace_file = arg;
      } else if (state->arg_num == 3) {
        arguments->trace_type_str = arg;
      } else if (state->arg_num == 4) {
        arguments->rate = atof(arg);
      } else if (state->arg_num > 4) {
        argp_usage(state);
      }
      break;
    case ARGP_KEY_END:
      if (state->arg_num < 3) {
        argp_usage(state);
      }
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

void parse_mrc_cmd(int argc, char **argv, struct PARAM *args) {
  struct SHARD_arguments arguments;
  /* Set default values */
  arguments.verver = false; // fixed-rate by default
  arguments.size = -1;
  arguments.rate = 0.0;
  arguments.trace_type_str = NULL;
  arguments.trace_type_params = NULL;
  arguments.ignore_obj_size = false;
  arguments.n_req = -1;
  
  argp_parse(&argp, argc, argv, 0, 0, &arguments);
  
  if (!arguments.trace_file) {
    fprintf(stderr, "Error: TRACE_FILE is missing.\n");
    exit(1);
  }
  if (!arguments.trace_type_str) {
    fprintf(stderr, "Error: TRACE_TYPE is missing.\n");
    exit(1);
  }
  
  if (arguments.verver) {
    printf("VER = fixed-size (threshold = %ld)\n", arguments.size);
    args->compute_distance = compute_distance_fixed_size;
  } else {
    printf("VER = fixed-rate\n");
    args->compute_distance = compute_distance_fixed_rate;
  }
  printf("TRACE_FILE = %s\nTRACE_TYPE = %s\nRATE = %f\n", arguments.trace_file, arguments.trace_type_str, arguments.rate);
  
  /* Set corresponding parameters in the PARAM structure */
  args->ver = arguments.verver;
  if (arguments.verver) {
    if (arguments.size <= 0) {
      fprintf(stderr, "Error: Size must be >0 for fixed-size mode.\n");
      exit(1);
    }
    args->threshold = arguments.size;
  } else {
    args->threshold = 0;  // fixed-rate mode
  }
  
  if (arguments.rate <= 0.0) {
    fprintf(stderr, "Error: Rate must be specified and >0.\n");
    exit(1);
  }
  args->rate = arguments.rate;
  
  arguments.trace_type = trace_type_str_to_enum(arguments.trace_type_str, arguments.trace_file);
  
  reader_init_param_t reader_init_params;
  memset(&reader_init_params, 0, sizeof(reader_init_params));
  reader_init_params.ignore_obj_size = arguments.ignore_obj_size;
  reader_init_params.ignore_size_zero_req = true;
  reader_init_params.cap_at_n_req = -1;
  reader_init_params.obj_id_is_num = true;
  if (arguments.n_req > 0) {
    reader_init_params.cap_at_n_req = arguments.n_req;
  }
  
  // Initialize the sampler by creating the shards sampler
  reader_init_params.sampler = create_SHARDS_sampler(arguments.rate);
  
  // Set the MRC algorithm
  args->mrc_algo = generate_shards_mrc;
  parse_reader_params(arguments.trace_type_params, &reader_init_params);
  
  if ((arguments.trace_type == CSV_TRACE || arguments.trace_type == PLAIN_TXT_TRACE) &&
      reader_init_params.obj_size_field == -1) {
    reader_init_params.ignore_obj_size = true;
  }
  
  args->reader = setup_reader(arguments.trace_file, arguments.trace_type, &reader_init_params);
}
