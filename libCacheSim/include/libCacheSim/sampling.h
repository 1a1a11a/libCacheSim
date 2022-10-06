#pragma once

#include "../../include/libCacheSim/request.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sampler;
struct request;

typedef bool (*trace_sampling_func)(struct sampler *sampler, request_t *req);

typedef struct sampler *(*clone_sampler_func)(const struct sampler *sampler);

typedef struct sampler *(*free_sampler_func)(struct sampler *sampler);

enum sampler_type {
  SPATIAL_SAMPLER,
  TEMPORAL_SAMPLER,

  INVALID_SAMPLER
};

static const char *sampling_type_str[] = {"spatial", "temporal", "invalid"};

typedef struct sampler {
  trace_sampling_func sample;
  int sampling_ratio_inv;
  double sampling_ratio;
  void *other_params;
  clone_sampler_func clone;
  free_sampler_func free;
  enum sampler_type type;
} sampler_t;

sampler_t *create_spatial_sampler(double sampling_ratio);

sampler_t *create_temporal_sampler(double sampling_ratio);

static inline void print_sampler(sampler_t *sampler) {
  printf("%s sampler: sample ratio %lf, sample func %p, clone func %p\n",
         sampling_type_str[sampler->type], sampler->sampling_ratio,
         sampler->sample, sampler->clone);
}

#ifdef __cplusplus
}
#endif
