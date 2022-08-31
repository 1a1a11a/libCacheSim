//
// Created by Juncheng on 5/29/21.
//

#include "../include/libCacheSim/admissionAlgo.h"
#include "../utils/include/mymath.h"

#ifdef __cplusplus
extern "C" {
#endif

struct prob_admissioner {
  double admission_probability;
  int admit_every_n;
};

bool prob_admit(void *prob_admissioner, request_t *req) {
  struct prob_admissioner *pa = prob_admissioner;
  if (next_rand() % pa->admit_every_n == 0) {
    return true;
  }

  return false;
}

void *create_prob_admissioner(double ratio) {
  if (ratio > 1 || ratio <= 0) {
    ERROR("prob admissioner ratio range error get %lf (should be 0-1)\n",
          ratio);
    abort();
  } else if (ratio == 1) {
    WARN("prob admission ratio 1\n");
  }

  struct prob_admissioner *s = my_malloc(struct prob_admissioner);
  memset(s, 0, sizeof(struct prob_admissioner));

  s->admission_probability = ratio;
  s->admit_every_n = (int)(1.0 / ratio);

  return s;
}

void free_prob_admissioner(void *s) {
  my_free(sizeof(struct prob_admissioner), s);
}

#ifdef __cplusplus
}
#endif
