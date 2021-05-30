#pragma once

#include "struct.h"
#include "reader.h"


#ifdef __cplusplus
extern "C" {
#endif

enum sampler_type {
  SPATIAL_SAMPLER,
  TEMPORAL_SAMPLER,

  INVALID_SAMPLER
};



typedef bool (*trace_sampling_func)(void *sampler, request_t *req);

static inline void add_sampling(struct reader *reader,
                  void *sampler,
                  trace_sampling_func func) {
  reader->sampler = sampler;
  reader->sample = func;
}




bool spatial_sample(void *sampler, request_t *req);
void* create_spatial_sampler(double ratio);
void free_spatial_sampler(void *s);

bool temporal_sample(void *sampler, request_t *req);
void* create_temporal_sampler(double ratio);
void free_temporal_sampler(void *s);


#ifdef __cplusplus
}
#endif

