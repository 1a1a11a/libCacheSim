#pragma once

#include <inttypes.h>

#include "../../include/libCacheSim/admissionAlgo.h"
#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define N_ARGS 4
#define N_MAX_ALGO 16
#define N_MAX_CACHE_SIZE 128
#define OFILEPATH_LEN 128

/* This structure is used to communicate with parse_opt. */
struct arguments {
  /* argument from the user */
  char *args[N_ARGS];
  char *trace_path;
  char *eviction_algo[N_MAX_ALGO];
  int n_eviction_algo;
  char *admission_algo;
  char *prefetch_algo;
  uint64_t cache_sizes[N_MAX_CACHE_SIZE];
  int n_cache_size;
  int warmup_sec;

  char ofilepath[OFILEPATH_LEN];
  char *trace_type_str;
  trace_type_e trace_type;
  char *trace_type_params;
  char *eviction_params;
  char *admission_params;
  char *prefetch_params;
  double sample_ratio;
  int n_thread;
  int64_t n_req; /* number of requests to process */

  bool verbose;
  int report_interval;
  bool ignore_obj_size;
  bool consider_obj_metadata;
  bool use_ttl;
  bool print_head_req;

  /* arguments generated */
  reader_t *reader;
  cache_t *caches[N_MAX_ALGO * N_MAX_CACHE_SIZE];
};

void parse_cmd(int argc, char *argv[], struct arguments *args);

void free_arg(struct arguments *args);

void simulate(reader_t *reader, cache_t *cache, int report_interval,
              int warmup_sec, char *ofilepath, bool ignore_obj_size,
              bool print_head_req);

void print_parsed_args(struct arguments *args);

#ifdef __cplusplus
}
#endif
