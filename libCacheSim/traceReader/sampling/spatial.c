/**
 * a spatial sampler that samples sampling_ratio of objects from the trace
 **/

#include "../../include/libCacheSim/logging.h"
#include "../../include/libCacheSim/sampling.h"
#include "../../dataStructure/hash/hash.h"

#ifdef __cplusplus
extern "C" {
#endif

bool spatial_sample(sampler_t *sampler, request_t *req) {
  uint64_t hash_value = req->hv;
  if (hash_value == 0) {
    hash_value = get_hash_value_int_64(&(req->obj_id));
    req->hv = hash_value;
  }

  return hash_value % sampler->sampling_ratio_inv == 0;
}

sampler_t *clone_spatial_sampler(const sampler_t *sampler) {
  sampler_t *cloned_sampler = my_malloc(sampler_t);
  memcpy(cloned_sampler, sampler, sizeof(sampler_t));

  VVERBOSE("clone spatial sampler\n");
  return cloned_sampler;
}

void free_spatial_sampler(sampler_t *sampler) { free(sampler); }

sampler_t *create_spatial_sampler(double sampling_ratio) {
  if (sampling_ratio > 1 || sampling_ratio <= 0) {
    ERROR("sampling ratio range error get %lf (should be 0-1)\n",
          sampling_ratio);
  } else if (sampling_ratio > 0.5) {
    ERROR("currently we only support sampling ratio no more than 0.5\n");
  } else if (sampling_ratio == 1) {
    WARN("spatial sampler ratio 1 means no sampling\n");
    return NULL;
  }

  sampler_t *s = my_malloc(sampler_t);
  memset(s, 0, sizeof(sampler_t));
  s->sampling_ratio = sampling_ratio;
  s->sampling_ratio_inv = (int)(1.0 / sampling_ratio);
  s->sample = spatial_sample;
  s->clone = clone_spatial_sampler;
  s->free = free_spatial_sampler;
  s->type = SPATIAL_SAMPLER;
  // s->sampling_boundary = (uint64_t)(ratio * UINT64_MAX);

  print_sampler(s);

  VVERBOSE("create spatial sampler with ratio %lf\n", sampling_ratio);
  return s;
}

#ifdef __cplusplus
}
#endif
