#pragma once

#include "libCacheSim/request.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sampler;
struct request;

typedef bool (*trace_sampling_func)(struct sampler *sampler, request_t *req);

typedef struct sampler *(*clone_sampler_func)(const struct sampler *sampler);

typedef void (*free_sampler_func)(struct sampler *sampler);

enum sampler_type {
  SPATIAL_SAMPLER,
  TEMPORAL_SAMPLER,
  SHARDS_SAMPLER,
  INVALID_SAMPLER
};

static const char *const sampling_type_str[] = {"spatial", "temporal", "shards",
                                                "invalid"};

typedef struct sampler {
  trace_sampling_func sample;
  int sampling_ratio_inv;
  double sampling_ratio;
  int sampling_salt;
  void *other_params;
  clone_sampler_func clone;
  free_sampler_func free;
  enum sampler_type type;
} sampler_t;

sampler_t *create_spatial_sampler(double sampling_ratio);

void set_spatial_sampler_salt(sampler_t *sampler, uint64_t salt);

sampler_t *create_temporal_sampler(double sampling_ratio);

static inline void print_sampler(sampler_t *sampler) {
  printf("%s sampler: sample ratio %lf\n", sampling_type_str[sampler->type],
         sampler->sampling_ratio);
}

sampler_t *create_SHARDS_sampler(double sampling_ratio);

#ifdef __cplusplus
}
#endif
