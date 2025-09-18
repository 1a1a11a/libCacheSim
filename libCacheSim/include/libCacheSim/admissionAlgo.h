/**
 * @file admissionAlgo.h
 * @brief Defines the interface and structures for cache admission policies.
 *
 * Admission policies are used to decide whether a new object that missed the cache
 * should be admitted into it. This file defines the `admissioner_t` structure,
 * which encapsulates the logic for an admission policy, and provides a factory
_func_ptr
 * function to create different admissioners.
 */

#pragma once

#include "request.h"

#ifdef __cplusplus
extern "C" {
#endif

struct admissioner;

/** @brief Function pointer to create and initialize an admissioner. */
typedef struct admissioner *(*admissioner_create_func_ptr)(const char *);

/** @brief Function pointer to clone an admissioner. */
typedef struct admissioner *(*admissioner_clone_func_ptr)(struct admissioner *);

/** @brief Function pointer to update the admissioner's state after a request. */
typedef void (*admissioner_update_func_ptr)(struct admissioner *, const request_t *, const uint64_t cache_size);

/** @brief Function pointer that decides whether to admit a request. */
typedef bool (*cache_admit_func_ptr)(struct admissioner *, const request_t *);

/** @brief Function pointer to free an admissioner. */
typedef void (*admissioner_free_func_ptr)(struct admissioner *);

#define CACHE_NAME_LEN 64

/**
 * @brief The main structure for a cache admission policy.
 *
 * This structure holds the function pointers and parameters that define the
 * behavior of an admission controller.
 */
typedef struct admissioner {
  cache_admit_func_ptr admit;             /**< Function to decide if an object should be admitted. */
  void *params;                           /**< A pointer to algorithm-specific parameters. */
  admissioner_clone_func_ptr clone;       /**< Function to clone the admissioner. */
  admissioner_free_func_ptr free;         /**< Function to free the admissioner. */
  admissioner_update_func_ptr update;     /**< Function to update internal state. */
  char *init_params;                      /**< The initialization parameter string. */
  char admissioner_name[CACHE_NAME_LEN];  /**< The name of the admission algorithm. */
} admissioner_t;

// Creation functions for specific admission algorithms
admissioner_t *create_bloomfilter_admissioner(const char *init_params);
admissioner_t *create_prob_admissioner(const char *init_params);
admissioner_t *create_size_admissioner(const char *init_params);
admissioner_t *create_size_probabilistic_admissioner(const char *init_params);
admissioner_t *create_adaptsize_admissioner(const char *init_params);

/**
 * @brief A factory function to create an admissioner based on a name.
 *
 * @param admission_algo The name of the admission algorithm (e.g., "bloomfilter", "size").
 * @param admission_params A string containing algorithm-specific parameters.
 * @return A pointer to a newly created `admissioner_t` instance, or NULL if the
 *         algorithm name is not recognized.
 */
static inline admissioner_t *create_admissioner(const char *admission_algo,
                                                const char *admission_params) {
  admissioner_t *admissioner = NULL;
  if (strcasecmp(admission_algo, "bloomfilter") == 0 ||
      strcasecmp(admission_algo, "bloom-filter") == 0) {
    admissioner = create_bloomfilter_admissioner(admission_params);
  } else if (strcasecmp(admission_algo, "prob") == 0 ||
             strcasecmp(admission_algo, "probabilistic") == 0) {
    admissioner = create_prob_admissioner(admission_params);
  } else if (strcasecmp(admission_algo, "size") == 0) {
    admissioner = create_size_admissioner(admission_params);
  } else if (strcasecmp(admission_algo, "sizeProbabilistic") == 0 ||
             strcasecmp(admission_algo, "sizeProb") == 0) {
    admissioner = create_size_probabilistic_admissioner(admission_params);
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
