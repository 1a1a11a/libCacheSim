//
// Created by Juncheng on 5/29/21.
//

#include "../../include/libCacheSim/admissionAlgo.h"
#include "../../utils/include/mymath.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MODULE 10000000

typedef struct prob_admissioner {
  double admission_probability;
  int admission_probability_int;
} prob_admission_params_t;

bool prob_admit(admissioner_t *admissioner, const request_t *req) {
  prob_admission_params_t *pa = (prob_admission_params_t *)admissioner->params;
  if (next_rand() % MAX_MODULE < pa->admission_probability_int) {
    return true;
  }

  return false;
}

static void prob_admissioner_parse_params(const char *init_params,
                                          prob_admission_params_t *pa) {
  if (init_params == NULL) {
    pa->admission_probability = 0.5;
    INFO("use default admission probability: %f\n", pa->admission_probability);
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

      if (strcasecmp(key, "prob") == 0) {
        pa->admission_probability = strtod(value, &end);
        if (strlen(end) > 2) {
          ERROR("param parsing error, find string \"%s\" after number\n", end);
        }
        INFO("use admission probability: %f\n", pa->admission_probability);
      } else {
        ERROR("probabilistic admission does not have parameter %s\n", key);
      }
    }
    free(old_params_str);
  }
  pa->admission_probability_int = pa->admission_probability * MAX_MODULE;

  if (pa->admission_probability > 1 || pa->admission_probability <= 0) {
    ERROR("prob admissioner probability error get %lf (should be 0-1)\n",
          pa->admission_probability);
  } else if (pa->admission_probability == 1) {
    WARN("prob admission probability 1\n");
  }
}

admissioner_t *clone_prob_admissioner(admissioner_t *admissioner) {
  return create_prob_admissioner(admissioner->init_params);
}

void free_prob_admissioner(admissioner_t *admissioner) {
  prob_admission_params_t *pa = admissioner->params;

  free(pa);
  if (admissioner->init_params) {
    free(admissioner->init_params);
  }
  free(admissioner);
}

admissioner_t *create_prob_admissioner(const char *init_params) {
  prob_admission_params_t *pa =
      (prob_admission_params_t *)malloc(sizeof(prob_admission_params_t));
  memset(pa, 0, sizeof(prob_admission_params_t));
  prob_admissioner_parse_params(init_params, pa);

  admissioner_t *admissioner = (admissioner_t *)malloc(sizeof(admissioner_t));
  memset(admissioner, 0, sizeof(admissioner_t));
  admissioner->params = pa;
  admissioner->admit = prob_admit;
  admissioner->free = free_prob_admissioner;
  admissioner->clone = clone_prob_admissioner;
  if (init_params != NULL) admissioner->init_params = strdup(init_params);

  strncpy(admissioner->admissioner_name, "Probabilistic", CACHE_NAME_LEN);
  return admissioner;
}

#ifdef __cplusplus
}
#endif
