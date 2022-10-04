#pragma once 

#include "../../../include/libCacheSim/evictionAlgo/GLCache.h"
#include "const.h"
#include "GLCacheInternal.h"


/* update the cache state */
static void update_cache_state(cache_t *cache, const request_t *req, cache_ck_res_e ck) {
  GLCache_params_t *params = cache->eviction_params;
  cache_state_t *state = &params->cache_state;
  if (unlikely(params->start_rtime == -1)) {
    params->start_rtime = req->real_time;
  }
  params->curr_rtime = req->real_time;
  params->curr_vtime++;

  if (ck == cache_ck_miss) {
    state->n_miss += 1;
    state->n_miss_bytes += req->obj_size;
  }

  int64_t rt = params->curr_rtime - state->last_update_rtime;
  int64_t vt = params->curr_vtime - state->last_update_vtime;
  if (rt > CACHE_STATE_UPDATE_RINTVL && vt > CACHE_STATE_UPDATE_VINTVL) {
    state->write_rate = (double) state->n_miss / rt;
    state->req_rate = (double) vt / rt;
    state->miss_ratio = (double) state->n_miss / vt;

    state->n_miss = 0;
    state->last_update_rtime = params->curr_rtime;
    state->last_update_vtime = params->curr_vtime;
  }
}
