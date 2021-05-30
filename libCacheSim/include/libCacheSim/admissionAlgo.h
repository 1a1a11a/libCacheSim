#pragma once

#include "struct.h"
#include "cache.h"


static inline void add_admission_algo(cache_t *cache, void *admissioner,
                                 cache_admission_func_ptr admission_func) {
  cache->admission_algo = admissioner;
  cache->admit = admission_func;
}

bool bloomfilter_admit(void *bloomfilter_admissioner, request_t *req);
void *create_bloomfilter_admissioner();
void free_bloomfilter_admisstioner(void *s);


bool prob_admit(void *prob_admissioner, request_t *req);
void *create_prob_admissioner(double ratio);
void free_prob_admisstioner(void *s);



