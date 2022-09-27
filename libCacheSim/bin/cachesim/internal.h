#pragma once

#include <inttypes.h>
#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/evictionAlgo.h"


typedef struct {
  char *trace_path;
  char *alg;
  trace_type_e trace_type;
  obj_id_type_e obj_id_type;
  int per_obj_overhead;

  int64_t cache_size_in_mb;
  struct {
    uint64_t cache_sizes[128];
    int n_cache_size;
  };

  int n_thread;
  bool debug;

#if defined(ENABLE_GLCache) && ENABLE_GLCache == 1
  struct {
    GLCache_type_e GLCache_type;
    int seg_size;
    int n_merge;
    double rank_intvl;
    bool merge_consecutive_segs; 
    train_source_e train_source_y;
    
    int age_shift;
    int min_start_train_seg;
    int max_start_train_seg;
    int n_train_seg_growth;
    int retrain_intvl;
    int sample_every_n_seg_for_training;
  };
#endif
  reader_t *reader;
  cache_t *cache;
} sim_arg_t;


#define N_ARGS 4

/* This structure is used to communicate with parse_opt. */
struct arguments {
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

  int per_obj_metadata_size;

  bool verbose;
  bool ignore_obj_size;
  bool use_ttl;
};


// sim_arg_t parse_cmd2(int argc, char *argv[]); 
