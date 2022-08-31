#include "../../include/libCacheSim/logging.h"
#include "../../include/libCacheSim/mem.h"
#include "../../include/libCacheSim/sampling.h"

struct temporal_sampler {
  double sampling_ratio;
  int n_samples;
  int sampling_every_n;
};

bool temporal_sample(void *sampler, request_t *req) {
  struct temporal_sampler *s = sampler;

  if (++s->n_samples == s->sampling_every_n) {
    s->n_samples = 0;
    return true;
  }

  return false;
}

void *create_temporal_sampler(double ratio) {
  struct temporal_sampler *s = my_malloc(struct temporal_sampler);
  memset(s, 0, sizeof(struct temporal_sampler));
  if (ratio > 1 || ratio <= 0) {
    ERROR("spatial sampler ratio range error get %lf (should be 0-1)\n", ratio);
    abort();
  } else if (ratio == 1) {
    WARNING("spatial sampler ratio 1\n");
  }

  s->sampling_ratio = ratio;
  s->n_samples = 0;
  s->sampling_every_n = (int)(1.0 / ratio);

  return s;
}

void free_temporal_sampler(void *s) {
  my_free(sizeof(struct temporal_sampler), s);
}
