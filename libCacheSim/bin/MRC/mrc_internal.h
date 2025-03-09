#pragma once

#include <glib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include "../../dataStructure/histogram.h"
#include "../../dataStructure/splay.h"
#include "../../dataStructure/splay_tuple.h"
#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/evictionAlgo.h"
#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/admissionAlgo.h"

#define N_ARGS 4
#define N_MAX_ALGO 16
#define N_MAX_CACHE_SIZE 128
#define OFILEPATH_LEN 128

// Forward declaration of struct Params
struct PARAM;


#ifdef __cplusplus
extern "C" {
#endif

struct SHARD_arguments {
  bool verver;
  long size;
  float rate;
  char* mrc_algo;
  char* trace_file;
  char *trace_type_str;
  trace_type_e trace_type;
  char* trace_type_params;
  bool ignore_obj_size;
  int64_t n_req;
};

struct PARAM {
  float rate;
  bool ver;  // 0 means fixed rate, 1 means fixed size
  
  uint64_t threshold;
  //GHashTable* prio_hash;
  sTree_tuple* prio_tree;  // root of the splay tree
  sTree* distance_tree;
  ReuseHistogram* data;
  GHashTable* lookup_hash;
  reader_t *reader;
  int64_t (*compute_distance)(struct PARAM *, request_t *, uint64_t);
  void (*mrc_algo)(struct PARAM*, char* path);
};

struct MINI_arguments {
  /* argument from the user */
  char *args[6];
  char *trace_path;
  char *eviction_algo[N_MAX_ALGO];
  int n_eviction_algo;
  char *admission_algo;
  char *prefetch_algo;
  uint64_t cache_sizes[N_MAX_CACHE_SIZE];
  float cache_size_ratio[N_MAX_CACHE_SIZE];
  int n_cache_size;
  int warmup_sec;

  char ofilepath[OFILEPATH_LEN];
  char *trace_type_str;
  trace_type_e trace_type;
  char *trace_type_params;
  char *eviction_params;
  char *admission_params;
  char *prefetch_params;
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

int64_t compute_distance_fixed_rate(struct PARAM *params, request_t *req, uint64_t timestamp);

int64_t compute_distance_fixed_size(struct PARAM *params, request_t *req, uint64_t timestamp);

void generate_shards_mrc(struct PARAM* params, char* path);

cache_stat_t * generate_mini_mrc(struct MINI_arguments* args);

void parse_mrc_cmd(int argc, char **argv, struct PARAM *args);

void parse_mini_cmd(int argc, char* argv[], struct MINI_arguments* args);

#ifdef __cplusplus
}
#endif