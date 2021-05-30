#include "../../include/libCacheSim/sampling.h"
#include "../../include/libCacheSim/logging.h"
#include "../../include/libCacheSim/mem.h"
#include "../dataStructure/hash/hash.h"

struct spatial_sampler {
    double sampling_ratio;
    uint64_t sampling_boundary;
};

bool spatial_sample(void *sampler, request_t *req) {
  struct spatial_sampler *s = sampler;
  uint64_t hash_value = req->hv;
  if (hash_value == 0) {
    hash_value = get_hash_value_int_64(&(req->obj_id));
    req->hv = hash_value;
  }

  if (hash_value <= s->sampling_boundary)
    return true;

  return false;
}

void* create_spatial_sampler(double ratio) {
  struct spatial_sampler *s = my_malloc(struct spatial_sampler);
  memset(s, 0, sizeof(struct spatial_sampler));
  if (ratio > 1 || ratio <= 0) {
    ERROR("spatial sampler ratio range error get %lf (should be 0-1)\n", ratio);
    abort();
  } else if (ratio == 1) {
    WARNING("spatial sampler ratio 1\n");
  }

  s->sampling_ratio = ratio;
  s->sampling_boundary = (uint64_t) (ratio * UINT64_MAX);

  return s;
}

void free_spatial_sampler(void *s) {
  my_free(sizeof(struct spatial_sampler), s);
}

