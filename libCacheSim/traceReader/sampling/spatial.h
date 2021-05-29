#pragma once

#include "../../include/libCacheSim/sampler.h"

struct spatial_sampler_state {
  ;
};

bool spatial_sample(sampler_t *s, request_t *req) {
  uint64_t hash_value = req->hv;
  if (hash_value == 0) {
    hash_value = get_hash_value_int_64(&(req->obj_id));
    req->hv = hash_value;
  }

  if (hash_value % s->sampling_ratio == 0)
    return true;

  return false;
}

sampler_t* create_spatial_sampler(int ratio) {
  sampler_t *s = my_malloc(sampler_t);
  memset(s, 0, sizeof(sampler_t));

  s->sample = spatial_sample;
  s->sampling_ratio = ratio;

  return s;
}

void free_spatial_sampler(sampler_t *s) {
  my_free(sizeof(sampler_t), s);
}

