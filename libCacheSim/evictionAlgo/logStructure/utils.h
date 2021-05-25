#pragma once

#include "../../include/libCacheSim/evictionAlgo/LLSC.h"


static inline bucket_t *next_unempty_bucket(cache_t *cache, int curr_idx) {
  LLSC_params_t *params = cache->cache_params;
  curr_idx = (curr_idx + 1) % MAX_N_BUCKET;
  while (params->buckets[curr_idx].n_seg < params->n_merge) {
    curr_idx = (curr_idx + 1) % MAX_N_BUCKET;
  }
  return &params->buckets[curr_idx];
}

static inline void update_cache_state(cache_t *cache) {
  LLSC_params_t *params = cache->cache_params;

  cache_state_t *state = &params->cache_state;
  int64_t rt = params->curr_rtime - state->last_update_rtime;
  int64_t vt = params->curr_vtime - state->last_update_vtime;
  if (rt >= CACHE_STATE_UPDATE_INTVL && vt > 10 * 1000) {
    state->write_rate = (double) state->n_miss / rt;
    state->req_rate = (double) vt / rt;
    state->write_ratio = (double) state->n_miss / vt;
    state->cold_miss_ratio = (double) state->n_miss / vt;

    state->n_miss = 0;
    state->last_update_rtime = params->curr_rtime;
    state->last_update_vtime = params->curr_vtime;
  }
}


static inline int count_hash_chain_len(cache_obj_t *cache_obj) {
  int n = 0;
  while (cache_obj) {
    n += 1;
    cache_obj = cache_obj->hash_next;
  }
  return n;
}




