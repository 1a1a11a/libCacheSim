#pragma once

#include "cache.h"


static inline void add_admission_algo(cache_t *cache, void *admissioner,
                                 cache_admission_func_ptr admission_func) {
  cache->admission_params = admissioner;
  cache->admit = admission_func;
}

bool bloomfilter_admit(void *bloomfilter_admissioner, request_t *req);
void *create_bloomfilter_admissioner();
void free_bloomfilter_admissioner(void *s);


bool prob_admit(void *prob_admissioner, request_t *req);
void *create_prob_admissioner(double ratio);
void free_prob_admissioner(void *s);

bool size_admit(void *size_admissioner, request_t *req);
void *create_size_admissioner(uint64_t size_threshold);
void free_size_admissioner(void *s);


