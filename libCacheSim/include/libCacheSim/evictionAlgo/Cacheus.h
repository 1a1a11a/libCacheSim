#pragma once

#include <glib.h>

#include "../cache.h"
#include "../evictionAlgo.h"
#include "inttypes.h"

typedef struct SR_LRU_params {
  cache_t *SR_list;    // Scan Resistant list
  cache_t *R_list;     // Churn Resistant List
  cache_t *H_list;     // History
  uint64_t C_demoted;  // count of demoted object in cache
  uint64_t C_new;      // count of new item in history
  cache_t *other_cache;
  request_t *req_local;
} SR_LRU_params_t;

typedef struct CR_LFU_params {
  freq_node_t *freq_one_node;
  GHashTable *freq_map;
  int64_t min_freq;
  int64_t max_freq;
  cache_t *other_cache;
  request_t *req_local;
} CR_LFU_params_t;
