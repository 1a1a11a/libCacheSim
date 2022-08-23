#pragma once

#include "request.h"
#include "reader.h"


#ifdef __cplusplus
extern "C" {
#endif

enum sampler_type {
  SPATIAL_SAMPLER,
  TEMPORAL_SAMPLER,

  INVALID_SAMPLER
};

struct spatial_sampler_params {
  int ratio;    // sample one of every ratio objects
};

struct temporal_sampler_params {
  int ratio;    // sample one of every ratio requests
};

typedef bool (*trace_sampling_func)(void *sampler_params, request_t *req);


bool spatial_sample(void *sampler_params, request_t *req);
bool temporal_sample(void *sampler_params, request_t *req);


#ifdef __cplusplus
}
#endif

