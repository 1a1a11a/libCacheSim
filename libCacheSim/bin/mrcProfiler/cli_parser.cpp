#include <argp.h>
#include <stdbool.h>
#include <string.h>

#include <string>
#include <vector>

#include "../cli_reader_utils.h"
#include "./internal.h"
#include "libCacheSim/const.h"
#include "mrcProfiler/mrcProfiler.h"
#include "utils/include/mystr.h"
#include "utils/include/mysys.h"

/**
 * @brief split string by char
 *
 * @param input
 * @param c
 */
std::vector<std::string> split_by_char(const char *input, const char &c) {
  std::vector<std::string> result;
  if (!input) return result;  // if input is null, return empty vector

  const char *start = input;
  const char *current = input;

  while (*current) {
    if (*current == c) {
      result.emplace_back(start, current - start);
      start = current + 1;
    }
    ++current;
  }

  if (current != start) result.emplace_back(start, current - start);
  return result;
}

#ifdef __cplusplus
extern "C" {
#endif

const char *argp_program_version = "mrcProfiler 0.0.1";
const char *argp_program_bug_address =
    "https://groups.google.com/g/libcachesim/";

enum argp_option_short {
  OPTION_TRACE_TYPE_PARAMS = 't',
  OPTION_OUTPUT_PATH = 'o',
  OPTION_NUM_REQ = 'n',
  OPTION_VERBOSE = 'v',

  /* profiler params */
  OPTION_CACHE_ALGORITHM = 0x100,
  OPTION_MRC_SIZE = 0x101,
  OPTION_PROFILER = 0x102,
  OPTION_PROFILER_PARAMS = 0x103,
  OPTION_IGNORE_OBJ_SIZE = 0x104,
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

    {NULL, 0, NULL, 0, "mrc profiler options:", 0},
    {"algo", OPTION_CACHE_ALGORITHM, "LRU", OPTION_ARG_OPTIONAL,
     "Which algorithm to profile. Only Support LRU for SHARDS.", 2},
    {"size", OPTION_MRC_SIZE, "0.01,1,100", OPTION_ARG_OPTIONAL,
     "MRC profile size. Support two formats "
     "[start_size,end_size,#test_points|size1,size2,size3,...,size_n]. For "
     "size settings, both explicit sizes (e.g., 1GiB) and WSS-based sizes (a "
     "floating-point number between 0 and 1) are supported.",
     2},
    {"profiler", OPTION_PROFILER, "SHARDS", OPTION_ARG_OPTIONAL,
     "Which profiler to use. Support SHARDS|MINISIM", 2},
    {"profiler-params", OPTION_PROFILER_PARAMS, "", OPTION_ARG_OPTIONAL,
     "Profiler parameters. ", 2},
    {"ignore-obj-size", OPTION_IGNORE_OBJ_SIZE, NULL, OPTION_ARG_OPTIONAL,
     "Ignore object size", 2},

    {NULL, 0, NULL, 0, "common parameters:", 0},

    {"output", OPTION_OUTPUT_PATH, "", OPTION_ARG_OPTIONAL, "Output path", 3},
    {"verbose", OPTION_VERBOSE, NULL, OPTION_ARG_OPTIONAL,
     "Produce verbose output", 3},
    {NULL, 0, NULL, 0, NULL, 0}};

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
      strncpy(arguments->ofilepath, arg, OFILEPATH_LEN - 1);
      arguments->ofilepath[OFILEPATH_LEN - 1] = '\0';
      break;
    case OPTION_NUM_REQ:
      arguments->n_req = atoll(arg);
      break;
    case OPTION_CACHE_ALGORITHM:
      arguments->cache_algorithm_str = arg;
      break;
    case OPTION_MRC_SIZE:
      arguments->mrc_size_str = arg;
      break;
    case OPTION_PROFILER:
      arguments->mrc_profiler_str = arg;
      break;
    case OPTION_PROFILER_PARAMS:
      arguments->mrc_profiler_params_str = arg;
      break;
    case OPTION_IGNORE_OBJ_SIZE:
      arguments->ignore_obj_size = true;
      break;
    case OPTION_VERBOSE:
      arguments->verbose = is_true(arg) ? true : false;
      break;
    case ARGP_KEY_ARG:
      if (state->arg_num >= N_ARGS) {
        ERROR("found too many arguments, current %s\n", arg);
        argp_usage(state);
        exit(1);
      }
      arguments->args[state->arg_num] = arg;
      break;
    case ARGP_KEY_END:
      if (state->arg_num < N_ARGS) {
        ERROR("not enough arguments found\n");
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
static char args_doc[] =
    "trace_path trace_type --algo=[LRU] --profiler=[SHARDS] "
    "--profiler-params=[FIX_RATE,0.01,hash_salt|FIX_SIZE,8192,hash_salt|FIX_"
    "RATE,0.01,thread_num(for MINISIM)] "
    "--size=[0.01,1,100|1MiB,100MiB,100|0.001,0.002,0.004,0.008,0.016|1MiB,"
    "10MiB,10MiB,1GiB]";

/* Program documentation. */
static char doc[] =
    "example: ./bin/mrcProfiler ../data/cloudPhysicsIO.vscsi vscsi --algo=LRU "
    "--profiler=SHARDS --profiler-params=FIX_RATE,0.01,42 --size=0.01,1,100\n\n"
    "trace_type: txt/csv/twr/vscsi/oracleGeneralBin and more\n"
    "if using csv trace, considering specifying -t obj-id-is-num=true\n"
    "algo: "
    "SHARDS only supports LRU, and MINISIM supports other eviction algorithms\n"
    "profiler: "
    "SHARDS or MINISIM\n"
    "profiler-params: "
    "only SHARDS support fix_size sampling\n"
    "size: "
    "profiling working set size related mrc or fixed size mrc\n\n";

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

/**
 * @brief Parse the MRC size string.
 *
 * In MRC profiling, it is necessary to support setting the cache size and the
 * number of test points. For setting the cache size, I referenced the
 * implementation in cachesim, which allows specifying a fixed cache size (e.g.,
 * 1GiB) or a Working Set Size (WSS)-based cache size (a float between 0 and 1).
 *
 * For setting the number of test points, the implementation supports Explicit
 * test points and Interval-Based points. For example, "1MiB,10MiB,10MiB,1GiB"
 * means that the cache size is 1MiB, 10MiB, 10MiB, and 1GiB, and the number of
 * test points is 4. The interval-based points are specified by a starting size,
 * an ending size, and a number of test points. For example, "1MiB,4MiB,4" means
 * that the cache size is 1MiB, 2MiB, 3MiB, and 4MiB, and the number of test
 * points is 4.
 *
 * Thus, the current implementation supports the following four input formats:
 *
 * |                        | Fixed cache size                | WSS based cache
 * size            |
 * |------------------------|---------------------------------|---------------------------------|
 * | Explicit test points   | "1MiB,10MiB,10MiB,1GiB"         |
 * "0.001,0.002,0.004,0.008,0.016" | | Interval-Based points  |
 * "1MiB,100MiB,100"               | "0.01,1,100"                    |
 *
 * @param mrc_size_str The MRC size string to parse.
 * @param params The structure to store the parsed MRC profiler parameters.
 */
static void parse_mrc_size_params(const char *mrc_size_str,
                                  mrcProfiler::mrc_profiler_params_t &params) {
  std::vector<std::string> mrc_size_vec = split_by_char(mrc_size_str, ',');

  // parse mrc size
  if (mrc_size_vec.size() == 0) {
    printf("mrc size must be set\n");
    exit(1);
  }

  bool wss_based_mrc = false;
  bool interval_based_mrc = false;
  // 1. check whether the first size is a double
  if (mrc_size_vec[0].find_first_not_of("0123456789.") == std::string::npos) {
    wss_based_mrc = true;
  }

  // 2. check whether the number of split strings is 3 and the last part is an
  // integer
  if (mrc_size_vec.size() == 3 &&
      mrc_size_vec[mrc_size_vec.size() - 1].find_first_not_of("0123456789") ==
          std::string::npos) {
    // if the last size is an integer and greater than 1, then it is interval
    // based mrc
    int64_t mrc_points = atoi(mrc_size_vec[mrc_size_vec.size() - 1].c_str());
    if (mrc_points > 1) {
      interval_based_mrc = true;
    }
  }

  if (interval_based_mrc) {
    int64_t mrc_points = atoi(mrc_size_vec[mrc_size_vec.size() - 1].c_str());
    mrc_size_vec.erase(mrc_size_vec.end() - 1);
    if (mrc_size_vec.size() != 2) {
      ERROR("mrc size setting wrong, current %s\n", mrc_size_str);
      exit(1);
    }
    if (wss_based_mrc) {
      double start_ratio = atof(mrc_size_vec[0].c_str());
      double end_ratio = atof(mrc_size_vec[1].c_str());
      if (start_ratio < 0 || end_ratio > 1 || start_ratio >= end_ratio) {
        ERROR("mrc start size or end size wrong, current %s\n", mrc_size_str);
        exit(1);
      }
      double interval = (end_ratio - start_ratio) / (mrc_points - 1);
      for (int i = 0; i < mrc_points - 1; i++) {
        double ratio = start_ratio + interval * i;
        params.profile_wss_ratio.push_back(ratio);
      }
      params.profile_wss_ratio.push_back(end_ratio);
    } else {
      uint64_t start_size =
          conv_size_str_to_byte_ul((char *)mrc_size_vec[0].c_str());
      uint64_t end_size =
          conv_size_str_to_byte_ul((char *)mrc_size_vec[1].c_str());
      if (start_size >= end_size) {
        ERROR("mrc start size or end size wrong, current %s\n", mrc_size_str);
        exit(1);
      }
      uint64_t interval = (end_size - start_size) / (mrc_points - 1);
      for (int i = 0; i < mrc_points - 1; i++) {
        uint64_t test_size = start_size + interval * i;
        params.profile_size.push_back(test_size);
      }
      params.profile_size.push_back(end_size);
    }
  } else {
    // not interval based mrc
    if (wss_based_mrc) {
      for (size_t i = 0; i < mrc_size_vec.size(); i++) {
        // must be double
        double ratio = atof(mrc_size_vec[i].c_str());
        if (ratio < 0 || ratio > 1) {
          ERROR("mrc wss ratio must be in [0, 1], current %s\n", mrc_size_str);
          exit(1);
        }
        params.profile_wss_ratio.push_back(ratio);
      }

      // cache size must be increasing
      for (size_t i = 0; i < params.profile_wss_ratio.size() - 1; i++) {
        if (params.profile_wss_ratio[i] >= params.profile_wss_ratio[i + 1]) {
          ERROR("mrc wss ratio must be increasing, current %s\n", mrc_size_str);
          exit(1);
        }
      }
    } else {
      for (size_t i = 0; i < mrc_size_vec.size(); i++) {
        uint64_t size =
            conv_size_str_to_byte_ul((char *)mrc_size_vec[i].c_str());
        params.profile_size.push_back(size);
      }
      // cache size must be increasing
      for (size_t i = 0; i < params.profile_size.size() - 1; i++) {
        if (params.profile_size[i] >= params.profile_size[i + 1]) {
          ERROR("mrc size must be increasing, current %s\n", mrc_size_str);
          exit(1);
        }
      }
    }
  }

  if (params.profile_size.size() > MAX_MRC_PROFILE_POINTS ||
      params.profile_wss_ratio.size() > MAX_MRC_PROFILE_POINTS) {
    ERROR("mrc size must be less than MAX_MRC_PROFILE_POINTS\n");
    exit(1);
  }
}

/**
 * @brief initialize the arguments.
 *
 * @param cache_algorithm_str
 * @param profiler_str
 * @param params_str
 * @param mrc_size_str
 * @param profiler_type
 * @param params
 */
void mrc_profiler_params_parse(const char *cache_algorithm_str,
                               const char *profiler_str, const char *params_str,
                               const char *mrc_size_str,
                               mrcProfiler::mrc_profiler_e &profiler_type,
                               mrcProfiler::mrc_profiler_params_t &params) {
  // initial the params of mrc profiler
  if (strcmp(profiler_str, "SHARDS") == 0 ||
      strcmp(profiler_str, "shards") == 0) {
    profiler_type = mrcProfiler::SHARDS_PROFILER;
    if (strcmp(cache_algorithm_str, "LRU")) {
      ERROR("cache algorithm must be LRU for SHARDS\n")
      exit(1);
    }

    params.cache_algorithm_str = (char *)cache_algorithm_str;

    // init shards params
    params.shards_params.parse_params(params_str);
  } else if (strcmp(profiler_str, "MINISIM") == 0 ||
             strcmp(profiler_str, "minisim") == 0) {
    profiler_type = mrcProfiler::MINISIM_PROFILER;

    params.cache_algorithm_str = (char *)cache_algorithm_str;

    // init minisim params
    params.minisim_params.parse_params(params_str);
  } else {
    ERROR("profiler type %s not supported\n", profiler_str);
    exit(1);
  }

  // parse mrc size
  parse_mrc_size_params(mrc_size_str, params);
}

/**
 * @brief initialize the arguments
 *
 * @param args
 */
static void init_arg(struct arguments *args) {
  // Initialize all fields directly instead of using memset
  args->trace_path = NULL;
  args->trace_type_params = NULL;
  memset(args->ofilepath, 0, OFILEPATH_LEN);
  args->n_req = -1;
  args->verbose = false;

  /* profiler params */
  args->ignore_obj_size = false;
  args->cache_algorithm_str = "LRU";
  args->mrc_size_str = "0.01,1,100";
  args->mrc_profiler_str = "SHARDS";
  args->mrc_profiler_params_str = "FIX_RATE,0.01,42";

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

  static struct argp argp = {.options = options,
                             .parser = parse_opt,
                             .args_doc = args_doc,
                             .doc = doc,
                             .children = NULL,
                             .help_filter = NULL,
                             .argp_domain = NULL};

  argp_parse(&argp, argc, argv, 0, 0, args);

  args->trace_path = args->args[0];
  const char *trace_type_str = args->args[1];

  // initialize the trace reader
  args->reader =
      create_reader(trace_type_str, args->trace_path, args->trace_type_params,
                    args->n_req, args->ignore_obj_size, 1);

  // initialize the mrc profiler params
  mrc_profiler_params_parse(args->cache_algorithm_str, args->mrc_profiler_str,
                            args->mrc_profiler_params_str, args->mrc_size_str,
                            args->mrc_profiler_type, args->mrc_profiler_params);

  if (args->mrc_profiler_params.profile_wss_ratio.size() != 0) {
    // need calcuate the working set size
    long wss = 0;
    int64_t wss_obj = 0, wss_byte = 0;
    cal_working_set_size(args->reader, &wss_obj, &wss_byte);
    wss = wss_byte;

    for (size_t i = 0; i < args->mrc_profiler_params.profile_wss_ratio.size();
         i++) {
      args->mrc_profiler_params.profile_size.push_back(
          wss * args->mrc_profiler_params.profile_wss_ratio[i]);
    }
  }
}

void free_arg(struct arguments *args) { close_reader(args->reader); }

#ifdef __cplusplus
}
#endif
