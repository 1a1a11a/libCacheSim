#pragma once

#include <inttypes.h>

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/dist.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/admissionAlgo.h"

#define N_ARGS 5
#define OFILEPATH_LEN 128

/* This structure is used to communicate with parse_opt. */
struct arguments {
  /* argument from the user */
  char *args[N_ARGS];
  char *trace_path;
  char ofilepath[OFILEPATH_LEN];
  char output_type[8];
  trace_type_e trace_type;
  dist_type_e dist_type;
  char *trace_type_params;
  int64_t n_req;    /* number of requests to process */
  bool verbose;

  /* arguments generated */
  reader_t *reader;
  cache_t *cache;
};

void parse_cmd(int argc, char *argv[], struct arguments *args);




