#pragma once

#include "../../include/libCacheSim/sampler.h"

struct temporal_sampler_state {
  ;
};

bool temporal_sample(sampler_t *s, request_t *req) {
  if (req->n_req % s->sampling_ratio == 0)
    return true;

  return false;
}

sampler_t* create_temporal_sampler(int ratio) {
  sampler_t *s = my_malloc(sampler_t);
  memset(s, 0, sizeof(sampler_t));

  s->sample = temporal_sample;
  s->sampling_ratio = ratio;

  return s;
}

void free_temporal_sampler(sampler_t *s) {
  my_free(sizeof(sampler_t), s);
}

