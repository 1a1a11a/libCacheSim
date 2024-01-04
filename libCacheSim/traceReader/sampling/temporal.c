/* a temporal sampler that samples every 1 / sampling_ratio requests */

#include <stdbool.h>

#include "../../include/libCacheSim/logging.h"
#include "../../include/libCacheSim/sampling.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct temporal_sampler_params {
  int n_samples;
} temporal_sampler_params_t;

bool temporal_sample(sampler_t *sampler, request_t *req) {
  temporal_sampler_params_t *params = sampler->other_params;

  if (++params->n_samples == sampler->sampling_ratio_inv) {
    params->n_samples = 0;
    return true;
  }

  return false;
}

sampler_t *clone_temporal_sampler(const sampler_t *sampler) {
  sampler_t *cloned_sampler = my_malloc(sampler_t);
  memcpy(cloned_sampler, sampler, sizeof(sampler_t));
  cloned_sampler->other_params = my_malloc(temporal_sampler_params_t);
  ((temporal_sampler_params_t *)cloned_sampler->other_params)->n_samples = 0;

  return cloned_sampler;
}

void free_temporal_sampler(sampler_t *sampler) {
  free(sampler->other_params);
  free(sampler);
}

sampler_t *create_temporal_sampler(double sampling_ratio) {
  if (sampling_ratio > 1 || sampling_ratio <= 0) {
    ERROR("sampling ratio range error get %lf (should be 0-1)\n",
          sampling_ratio);
  } else if (sampling_ratio > 0.5) {
    ERROR("currently we only support sampling ratio no more than 0.5\n");
  } else if (sampling_ratio == 1) {
    WARN("temporal sampler ratio 1 means no sampling\n");
    return NULL;
  }

  sampler_t *s = my_malloc(sampler_t);
  memset(s, 0, sizeof(sampler_t));
  s->sampling_ratio = sampling_ratio;
  s->sampling_ratio_inv = (int)(1.0 / sampling_ratio);
  s->sample = temporal_sample;
  s->clone = clone_temporal_sampler;
  s->free = free_temporal_sampler;
  s->type = TEMPORAL_SAMPLER;

  s->other_params = my_malloc(temporal_sampler_params_t);
  ((temporal_sampler_params_t *)s->other_params)->n_samples = 0;

  return s;
}

#ifdef __cplusplus
}
#endif
