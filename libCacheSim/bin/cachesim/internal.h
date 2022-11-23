#pragma once

#include <inttypes.h>

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/admissionAlgo.h"

#define N_ARGS 4
#define N_MAX_CACHE_SIZE 128
#define OFILEPATH_LEN 128

/* This structure is used to communicate with parse_opt. */
struct arguments {
  /* argument from the user */
  char *args[N_ARGS];
  char *trace_path;
  char *eviction_algo;
  char *admission_algo;
  uint64_t cache_sizes[N_MAX_CACHE_SIZE];
  int n_cache_size;
  int warmup_sec;

  char ofilepath[OFILEPATH_LEN];
  char *trace_type_str;
  trace_type_e trace_type;
  char *trace_type_params;
  char *eviction_params;
  char *admission_params;
  double sample_ratio;
  int n_thread;
  int64_t n_req;    /* number of requests to process */

  bool verbose;
  bool ignore_obj_size;
  bool consider_obj_metadata;
  bool use_ttl;

  /* arguments generated */
  reader_t *reader;
  cache_t *cache;
};

void parse_cmd(int argc, char *argv[], struct arguments *args);

void simulate(reader_t *reader, cache_t *cache, int warmup_sec,
              char *ofilepath);

bool set_hard_code_cache_size(struct arguments *args);

void parse_reader_params(char *reader_params_str, reader_init_param_t *params);

int conv_cache_sizes(char *cache_size_str, struct arguments *args);

void print_parsed_args(struct arguments *args);

void set_cache_size(struct arguments *args, reader_t *reader);

bool is_true(const char *arg); 
