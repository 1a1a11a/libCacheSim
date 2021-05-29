#pragma once


enum sampler_type {
  SPATIAL_SAMPLER,
  TEMPORAL_SAMPLER,

  INVALID_SAMPLER
};

struct sampler {
  bool (*sample)(struct sampler *sampler, request_t *req);
  void *sampler_state;
  int32_t sampling_ratio;
};

typedef struct sampler sampler_t;
