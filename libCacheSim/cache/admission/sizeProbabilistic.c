/**
 * @file sizeProbabilistic.c
 * @brief Implements a size-aware probabilistic admission policy.
 *
 * This admission policy combines ideas from both size-based and probabilistic
 * admission. The probability of admitting a new object is a function of its
 * size, with larger objects having a lower probability of being admitted.
 * The admission probability is calculated as `exp(-exponent * object_size)`,
 * where `exponent` is a configurable parameter.
 */

#include <math.h>

#include "libCacheSim/admissionAlgo.h"
#include "utils/include/mymath.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MODULE 10000000

/**
 * @brief Parameters for the size-probabilistic admissioner.
 */
typedef struct size_probabilistic_admissioner {
  double exponent; /**< The exponent used in the probability calculation. */
} size_probabilistic_admission_params_t;

/**
 * @brief Decides whether to admit a request based on a size-dependent probability.
 *
 * @param admissioner The admissioner instance.
 * @param req The request to consider.
 * @return True to admit the object, false otherwise.
 */
bool size_probabilistic_admit(admissioner_t *admissioner,
                              const request_t *req) {
  size_probabilistic_admission_params_t *pa =
      (size_probabilistic_admission_params_t *)admissioner->params;
  double prob = exp(-pa->exponent * (double)req->obj_size);
  if ((double)(next_rand() % MAX_MODULE) / (double)MAX_MODULE < prob) {
    return true;
  }
  return false;
}

/**
 * @brief Parses the initialization string for the size-probabilistic admissioner.
 *
 * Expected parameter: "exponent=<value>", where value is the exponent.
 *
 * @param init_params The string of initialization parameters.
 * @param pa A pointer to the parameter struct to be filled.
 */
static void size_probabilistic_admissioner_parse_params(
    const char *init_params, size_probabilistic_admission_params_t *pa) {
  if (init_params == NULL) {
    pa->exponent = 1e-6;
  } else {
    char *p_params = strdup(init_params);
    char *tok = strtok(p_params, ",");
    while(tok != NULL) {
        char* key = strsep(&tok, "=");
        char* value = tok;
        if (strcasecmp(key, "exponent") == 0) {
            pa->exponent = atof(value);
        } else {
            ERROR("size-probabilistic admission does not have parameter %s\n", key);
        }
        tok = strtok(NULL, ",");
    }
    free(p_params);
  }

  if (pa->exponent <= 0) {
    ERROR("exponent must be positive, but got %lf\n", pa->exponent);
  }
}

/**
 * @brief Clones a size-probabilistic admissioner instance.
 */
admissioner_t *clone_size_probabilistic_admissioner(
    admissioner_t *admissioner) {
  return create_size_probabilistic_admissioner(admissioner->init_params);
}

/**
 * @brief Frees the resources used by a size-probabilistic admissioner.
 */
void free_size_probabilistic_admissioner(admissioner_t *admissioner) {
  size_probabilistic_admission_params_t *pa = admissioner->params;
  free(pa);
  if (admissioner->init_params) {
    free(admissioner->init_params);
  }
  free(admissioner);
}

/**
 * @brief Creates and initializes a new size-probabilistic admissioner.
 * @param init_params Initialization parameters, e.g., "exponent=1e-6".
 * @return A pointer to the newly created admissioner.
 */
admissioner_t *create_size_probabilistic_admissioner(const char *init_params) {
  size_probabilistic_admission_params_t *pa =
      (size_probabilistic_admission_params_t *)malloc(
          sizeof(size_probabilistic_admission_params_t));
  size_probabilistic_admissioner_parse_params(init_params, pa);

  admissioner_t *admissioner = (admissioner_t *)malloc(sizeof(admissioner_t));
  admissioner->params = pa;
  admissioner->admit = size_probabilistic_admit;
  admissioner->free = free_size_probabilistic_admissioner;
  admissioner->clone = clone_size_probabilistic_admissioner;
  if (init_params != NULL) admissioner->init_params = strdup(init_params);

  strncpy(admissioner->admissioner_name, "SizeProbabilistic",
          CACHE_NAME_LEN - 1);
  admissioner->admissioner_name[CACHE_NAME_LEN - 1] = '\0';
  return admissioner;
}

#ifdef __cplusplus
}
#endif
