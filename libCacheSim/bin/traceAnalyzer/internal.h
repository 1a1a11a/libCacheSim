#pragma once

#include <inttypes.h>

#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/reader.h"
#include "../../traceAnalyzer/analyzer.h"

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

  traceAnalyzer::analysis_option_t analysis_option;
  traceAnalyzer::analysis_param_t analysis_param;

  /* arguments generated */
  reader_t *reader;
};

void parse_cmd(int argc, char *argv[], struct arguments *args);

#ifdef __cplusplus
}
#endif
