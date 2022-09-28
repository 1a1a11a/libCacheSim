#pragma once

#include <inttypes.h>

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/reader.h"

#define N_ARGS 4
#define N_MAX_CACHE_SIZE 128
#define N_AUTO_CACHE_SIZE 8

/* This structure is used to communicate with parse_opt. */
struct arguments {
  /* argument from the user */
  char *args[N_ARGS];
  char *trace_path;
  char *eviction_algo;
  char *admission_algo;
  unsigned long cache_size;
  unsigned long warmup_sec;

  char *ofilepath;
  char *trace_type_str;
  trace_type_e trace_type;
  char *trace_type_params;
  char *eviction_params;
  char *admission_params;
  int n_thread;

  bool verbose;
  bool ignore_obj_size;
  bool consider_obj_metadata;
  bool use_ttl;

  /* arguments generated */
  reader_t *reader;
  cache_t *cache;
  uint64_t cache_sizes[N_MAX_CACHE_SIZE];
  int n_cache_size;
};

void parse_cmd(int argc, char *argv[], struct arguments *args);

bool set_hard_code_cache_size(struct arguments *args);

void simulate(reader_t *reader, cache_t *cache, int warmup_sec, char *ofilepath);
