#pragma once

#include "request.h"

#ifdef __cplusplus
extern "C" {
#endif

struct admissioner;
typedef struct admissioner *(*admissioner_create_func_ptr)(const char *);
typedef struct admissioner *(*admissioner_clone_func_ptr)(struct admissioner *);
typedef bool (*cache_admit_func_ptr)(struct admissioner*, const request_t *);
typedef void (*admissioner_free_func_ptr)(struct admissioner *);

typedef struct admissioner {
  cache_admit_func_ptr admit;
  void *params;
  admissioner_clone_func_ptr clone;
  admissioner_free_func_ptr free;
  void *init_params;
} admissioner_t;

admissioner_t *create_bloomfilter_admissioner(const char *init_params);
admissioner_t *create_prob_admissioner(const char *init_params);
admissioner_t *create_size_admissioner(const char *init_params);
admissioner_t *create_adaptsize_admissioner(const char *init_params);

static inline admissioner_t *create_admissioner(const char *admission_algo,
                                                const char *admission_params) {
  admissioner_t *admissioner = NULL;
  if (strcasecmp(admission_algo, "bloomfilter") == 0 || strcasecmp(admission_algo, "bloom-filter") == 0) {
    admissioner = create_bloomfilter_admissioner(admission_params);
  } else if (strcasecmp(admission_algo, "prob") == 0) {
    admissioner = create_prob_admissioner(admission_params);
  } else if (strcasecmp(admission_algo, "size") == 0) {
    admissioner = create_size_admissioner(admission_params);
  } else if (strcasecmp(admission_algo, "adaptsize") == 0) {
    admissioner = create_adaptsize_admissioner(admission_params);
  } else {
    ERROR("admission algo %s not supported\n", admission_algo);
  }

  return admissioner;
}

#ifdef __cplusplus
}
#endif