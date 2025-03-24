#pragma once

#include <inttypes.h>

#include "../../mrcProfiler/mrcProfiler.h"
#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/reader.h"

#define N_ARGS 2
#define OFILEPATH_LEN 128

#ifdef __cplusplus
extern "C" {
#endif

/* This structure is used to communicate with parse_opt. */
struct arguments {
  /* argument from the user */
  char *args[N_ARGS];
  char *trace_path;
  trace_type_e trace_type;
  char *trace_type_params;
  char ofilepath[OFILEPATH_LEN];
  int64_t n_req; /* number of requests to process */
  bool verbose;

  /* profiler params */
  bool ignore_obj_size;
  char *cache_algorithm_str;
  char *mrc_size_str;
  char *mrc_profiler_str;
  char *mrc_profiler_params_str;

  mrcProfiler::mrc_profiler_e mrc_profiler_type;
  mrcProfiler::mrc_profiler_params_t mrc_profiler_params;

  /* arguments generated */
  reader_t *reader;
};

void parse_cmd(int argc, char *argv[], struct arguments *args);

#ifdef __cplusplus
}
#endif
