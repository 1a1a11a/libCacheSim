#pragma once 

#include "../../include/libCacheSim/evictionAlgo/L2Cache.h"
#include "const.h"
#include "L2CacheInternal.h"


/* update the cache state */
static inline void update_cache_state(cache_t *cache, request_t *req) {
  L2Cache_params_t *params = cache->eviction_params;
  cache_state_t *state = &params->cache_state;
  if (unlikely(params->start_rtime == 0)) {
    params->start_rtime = req->real_time;
  }
  params->curr_rtime = req->real_time;
  params->curr_vtime++;

  int64_t rt = params->curr_rtime - state->last_update_rtime;
  int64_t vt = params->curr_vtime - state->last_update_vtime;
  if (rt >= CACHE_STATE_UPDATE_RINTVL && vt > CACHE_STATE_UPDATE_VINTVL) {
    state->write_rate = (double) state->n_miss / rt;
    state->req_rate = (double) vt / rt;
    state->miss_ratio = (double) state->n_miss / vt;

    state->n_miss = 0;
    state->last_update_rtime = params->curr_rtime;
    state->last_update_vtime = params->curr_vtime;
  }
}
