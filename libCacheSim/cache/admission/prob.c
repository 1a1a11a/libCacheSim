/**
 * @file prob.c
 * @brief Implementation of a probabilistic admission policy.
 *
 * This admission policy admits new objects into the cache based on a fixed,
 * user-configurable probability. For each cache miss, a random number is
 * generated and compared against the admission probability to decide whether
 * the new object should be inserted into the cache.
 */

#include "libCacheSim/admissionAlgo.h"
#include "utils/include/mymath.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MODULE 10000000

/**
 * @brief Parameters for the probabilistic admissioner.
 */
typedef struct prob_admissioner {
  double admission_probability;     /**< The probability (0.0 to 1.0) of admitting a new object. */
  int admission_probability_int;  /**< The probability scaled to an integer for efficient comparison. */
} prob_admission_params_t;

/**
 * @brief Decides whether to admit a request based on a fixed probability.
 *
 * @param admissioner The admissioner instance.
 * @param req The request to consider (not used in this policy).
 * @return True to admit the object, false otherwise.
 */
bool prob_admit(admissioner_t *admissioner, const request_t *req) {
  prob_admission_params_t *pa = (prob_admission_params_t *)admissioner->params;
  if ((int)(next_rand() % MAX_MODULE) < pa->admission_probability_int) {
    return true;
  }
  return false;
}

/**
 * @brief Parses the initialization string for the probabilistic admissioner.
 *
 * Expected parameter: "prob=<value>", where value is a float between 0 and 1.
 *
 * @param init_params The string of initialization parameters.
 * @param pa A pointer to the parameter struct to be filled.
 */
static void prob_admissioner_parse_params(const char *init_params,
                                          prob_admission_params_t *pa) {
  if (init_params == NULL) {
    pa->admission_probability = 0.5;
  } else {
    char *p_params = strdup(init_params);
    char *tok = strtok(p_params, ",");
    while(tok != NULL) {
        char* key = strsep(&tok, "=");
        char* value = tok;
        if (strcasecmp(key, "prob") == 0) {
            pa->admission_probability = atof(value);
        } else {
            ERROR("probabilistic admission does not have parameter %s\n", key);
        }
        tok = strtok(NULL, ",");
    }
    free(p_params);
  }
  pa->admission_probability_int = pa->admission_probability * MAX_MODULE;

  if (pa->admission_probability > 1 || pa->admission_probability <= 0) {
    ERROR("prob admissioner probability error get %lf (should be 0-1)\n",
          pa->admission_probability);
  }
}

/**
 * @brief Clones a probabilistic admissioner instance.
 */
admissioner_t *clone_prob_admissioner(admissioner_t *admissioner) {
  return create_prob_admissioner(admissioner->init_params);
}

/**
 * @brief Frees the resources used by a probabilistic admissioner.
 */
void free_prob_admissioner(admissioner_t *admissioner) {
  prob_admission_params_t *pa = admissioner->params;
  free(pa);
  if (admissioner->init_params) {
    free(admissioner->init_params);
  }
  free(admissioner);
}

/**
 * @brief Creates and initializes a new probabilistic admissioner.
 * @param init_params Initialization parameters, e.g., "prob=0.1".
 * @return A pointer to the newly created admissioner.
 */
admissioner_t *create_prob_admissioner(const char *init_params) {
  prob_admission_params_t *pa =
      (prob_admission_params_t *)malloc(sizeof(prob_admission_params_t));
  prob_admissioner_parse_params(init_params, pa);

  admissioner_t *admissioner = (admissioner_t *)malloc(sizeof(admissioner_t));
  admissioner->params = pa;
  admissioner->admit = prob_admit;
  admissioner->free = free_prob_admissioner;
  admissioner->clone = clone_prob_admissioner;
  if (init_params != NULL) admissioner->init_params = strdup(init_params);

  strncpy(admissioner->admissioner_name, "Probabilistic", CACHE_NAME_LEN - 1);
  admissioner->admissioner_name[CACHE_NAME_LEN - 1] = '\0';
  return admissioner;
}

#ifdef __cplusplus
}
#endif
