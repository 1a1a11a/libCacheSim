/**
 * a spatial sampler that samples sampling_ratio of objects from the trace
 **/

#include "../../dataStructure/hash/hash.h"
#include "../../include/libCacheSim/logging.h"
#include "../../include/libCacheSim/sampling.h"

#ifdef __cplusplus
extern "C" {
#endif

bool spatial_sample(sampler_t *sampler, request_t *req) {
  static const uint64_t MODULES = 1 << 18;
  uint64_t hash_value = req->hv;
  if (hash_value == 0) {
    hash_value = get_hash_value_int_64(&(req->obj_id));
    req->hv = hash_value;
  }

  bool is_sampled = (hash_value % MODULES) < (sampler->sampling_ratio * MODULES);
  sampler->total_count += 1;
  sampler->access_count += is_sampled;
  return is_sampled;
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
    ERROR("sampling ratio range error get %lf (should be 0-1)\n", sampling_ratio);
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
  s->total_count = 0;
  s->access_count = 0;
  // s->sampling_boundary = (uint64_t)(ratio * UINT64_MAX);

  print_sampler(s);

  VVERBOSE("create spatial sampler with ratio %lf\n", sampling_ratio);
  return s;
}

double scale_miss_ratio(sampler_t *sampler, double miss_ratio) {
  if (sampler->type == SPATIAL_SAMPLER) {
    ssize_t total_count = sampler->total_count;
    ssize_t access_count = sampler->access_count;
    ssize_t expected_count = total_count * sampler->sampling_ratio;
    return (double)(miss_ratio * access_count) / (double)expected_count;
  } else {
    ERROR("Invalid sampler type for scaling miss ratio\n");
    return -1;  // Return an error value for invalid sampler types
  }
}

#ifdef __cplusplus
}
#endif
