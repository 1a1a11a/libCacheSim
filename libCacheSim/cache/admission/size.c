/**
 * @file size.c
 * @brief Implementation of a size-based admission policy.
 *
 * This admission policy only admits objects into the cache if their size is
 * less than a user-configurable threshold. This can be used to prevent very
 * large objects from evicting many smaller objects (cache thrashing).
 */

#include "libCacheSim/admissionAlgo.h"
#include "utils/include/mymath.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parameters for the size admissioner.
 */
typedef struct size_admissioner {
  int64_t size_threshold; /**< The maximum size in bytes for an object to be admitted. */
} size_admission_params_t;

/**
 * @brief Decides whether to admit a request based on its object size.
 *
 * @param admissioner The admissioner instance.
 * @param req The request to consider.
 * @return True if the request's object size is less than the threshold, false otherwise.
 */
bool size_admit(admissioner_t *admissioner, const request_t *req) {
  size_admission_params_t *pa = (size_admission_params_t *)admissioner->params;
  if (req->obj_size < pa->size_threshold) {
    return true;
  }
  return false;
}

/**
 * @brief Parses the initialization string for the size admissioner.
 *
 * Expected parameter: "size=<value>", where value is the size threshold in bytes.
 *
 * @param init_params The string of initialization parameters.
 * @param pa A pointer to the parameter struct to be filled.
 */
static void size_admissioner_parse_params(const char *init_params,
                                          size_admission_params_t *pa) {
  if (init_params == NULL) {
    pa->size_threshold = INT64_MAX;
  } else {
    char *p_params = strdup(init_params);
    char *tok = strtok(p_params, ",");
    while(tok != NULL) {
        char* key = strsep(&tok, "=");
        char* value = tok;
        if (strcasecmp(key, "size") == 0) {
            pa->size_threshold = atol(value);
        } else {
            ERROR("size admission does not have parameter %s\n", key);
        }
        tok = strtok(NULL, ",");
    }
    free(p_params);
  }
}

/**
 * @brief Clones a size admissioner instance.
 */
admissioner_t *clone_size_admissioner(admissioner_t *admissioner) {
  return create_size_admissioner(admissioner->init_params);
}

/**
 * @brief Frees the resources used by a size admissioner.
 */
void free_size_admissioner(admissioner_t *admissioner) {
  size_admission_params_t *pa = admissioner->params;
  free(pa);
  if (admissioner->init_params) {
    free(admissioner->init_params);
  }
  free(admissioner);
}

/**
 * @brief Creates and initializes a new size admissioner.
 * @param init_params Initialization parameters, e.g., "size=1048576".
 * @return A pointer to the newly created admissioner.
 */
admissioner_t *create_size_admissioner(const char *init_params) {
  size_admission_params_t *pa =
      (size_admission_params_t *)malloc(sizeof(size_admission_params_t));
  size_admissioner_parse_params(init_params, pa);

  admissioner_t *admissioner = (admissioner_t *)malloc(sizeof(admissioner_t));
  admissioner->params = pa;
  admissioner->admit = size_admit;
  admissioner->free = free_size_admissioner;
  admissioner->clone = clone_size_admissioner;
  if (init_params != NULL) admissioner->init_params = strdup(init_params);

  strncpy(admissioner->admissioner_name, "Size", CACHE_NAME_LEN - 1);
  admissioner->admissioner_name[CACHE_NAME_LEN - 1] = '\0';
  return admissioner;
}

#ifdef __cplusplus
}
#endif
