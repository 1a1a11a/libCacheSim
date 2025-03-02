//
// Created by Juncheng on 10/29/24.
//
// size-probabilistic admission is a probabilistic admission that
// also considers object size, larger objects have lower probabilities
// to be admitted
// the probability for admitting an object of size S is e^(-exponent * S)
//

#include <math.h>

#include "../../include/libCacheSim/admissionAlgo.h"
#include "../../utils/include/mymath.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MODULE 10000000

typedef struct size_probabilistic_admissioner {
  double exponent;
} size_probabilistic_admission_params_t;

bool size_probabilistic_admit(admissioner_t *admissioner, const request_t *req) {
  size_probabilistic_admission_params_t *pa = (size_probabilistic_admission_params_t *)admissioner->params;
  double prob = exp(-pa->exponent * (double)req->obj_size);
  if ((double)(next_rand() % MAX_MODULE) / (double)MAX_MODULE < prob) {
    return true;
  }

  return false;
}

static void size_probabilistic_admissioner_parse_params(const char *init_params,
                                                        size_probabilistic_admission_params_t *pa) {
  if (init_params == NULL) {
    pa->exponent = 1e-6;
    INFO("use default admission exponent: %f\n", pa->exponent);
  } else {
    char *params_str = strdup(init_params);
    char *old_params_str = params_str;
    char *end;

    while (params_str != NULL && params_str[0] != '\0') {
      /* different parameters are separated by comma,
       * key and value are separated by = */
      char *key = strsep((char **)&params_str, "=");
      char *value = strsep((char **)&params_str, ",");

      // skip the white space
      while (params_str != NULL && *params_str == ' ') {
        params_str++;
      }

      if (strcasecmp(key, "exponent") == 0) {
        pa->exponent = strtod(value, &end);
        if (strlen(end) > 2) {
          ERROR("param parsing error, find string \"%s\" after number\n", end);
        }
        INFO("use admission exponent: %f\n", pa->exponent);
      } else {
        ERROR("size-probabilistic admission does not have parameter %s\n", key);
      }
    }
    free(old_params_str);
  }

  if (pa->exponent > 1 || pa->exponent <= 0) {
    ERROR(
        "size-probabilistic admissioner calculates probability e^(-exponent * obj_size) to admit object, a common "
        "exponent should be 0-1, e.g., 1e-6, but input %lf\n",
        pa->exponent);
  }
}

admissioner_t *clone_size_probabilistic_admissioner(admissioner_t *admissioner) {
  return create_size_probabilistic_admissioner(admissioner->init_params);
}

void free_size_probabilistic_admissioner(admissioner_t *admissioner) {
  size_probabilistic_admission_params_t *pa = admissioner->params;

  free(pa);
  if (admissioner->init_params) {
    free(admissioner->init_params);
  }
  free(admissioner);
}

admissioner_t *create_size_probabilistic_admissioner(const char *init_params) {
  size_probabilistic_admission_params_t *pa =
      (size_probabilistic_admission_params_t *)malloc(sizeof(size_probabilistic_admission_params_t));
  memset(pa, 0, sizeof(size_probabilistic_admission_params_t));
  size_probabilistic_admissioner_parse_params(init_params, pa);

  admissioner_t *admissioner = (admissioner_t *)malloc(sizeof(admissioner_t));
  memset(admissioner, 0, sizeof(admissioner_t));
  admissioner->params = pa;
  admissioner->admit = size_probabilistic_admit;
  admissioner->free = free_size_probabilistic_admissioner;
  admissioner->clone = clone_size_probabilistic_admissioner;
  if (init_params != NULL) admissioner->init_params = strdup(init_params);

  strncpy(admissioner->admissioner_name, "SizeProbabilistic", CACHE_NAME_LEN);
  return admissioner;
}

#ifdef __cplusplus
}
#endif
