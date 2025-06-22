/**
 * a spatial sampler that samples sampling_ratio of objects from the trace
 **/

#include "dataStructure/hash/hash.h"
#include "libCacheSim/logging.h"
#include "libCacheSim/sampling.h"

#ifdef __cplusplus
extern "C" {
#endif

// Fixed-size SHARDS Sampling

bool SHARDS_sample(sampler_t *sampler, request_t *req) {
  uint64_t hash_value = req->hv;
  if (hash_value == 0) {
    hash_value = get_hash_value_int_64(&(req->obj_id));
    req->hv = hash_value;
  }

  bool result = (hash_value & ((1 << 24) - 1)) <
                (uint64_t)(sampler->sampling_ratio * (1 << 24) + 0.5);

  return result;
}

sampler_t *clone_SHARDS_sampler(const sampler_t *sampler) {
  sampler_t *cloned_sampler = my_malloc(sampler_t);
  memcpy(cloned_sampler, sampler, sizeof(sampler_t));

  VERBOSE("clone SHARDS sampler\n");
  return cloned_sampler;
}

void free_SHARDS_sampler(sampler_t *sampler) { free(sampler); }

sampler_t *create_SHARDS_sampler(double sampling_ratio) {
  if (sampling_ratio > 1 || sampling_ratio <= 0) {
    ERROR("sampling ratio range error get %lf (should be 0-1)\n",
          sampling_ratio);
  }

  sampler_t *s = my_malloc(sampler_t);
  memset(s, 0, sizeof(sampler_t));
  s->sampling_ratio = sampling_ratio;
  s->sampling_ratio_inv = (int)(1.0 / sampling_ratio);
  s->sample = SHARDS_sample;
  s->clone = clone_SHARDS_sampler;
  s->free = free_SHARDS_sampler;
  s->type = SHARDS_SAMPLER;

  print_sampler(s);

  VERBOSE("create SHARDS sampler with ratio %lf\n", sampling_ratio);
  return s;
}

#ifdef __cplusplus
}
#endif
