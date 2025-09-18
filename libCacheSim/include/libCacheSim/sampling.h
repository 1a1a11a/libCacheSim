/**
 * @file sampling.h
 * @brief Defines the interface and structures for trace sampling algorithms.
 *
 * Trace sampling is used to reduce the number of requests that need to be
 * processed, which can significantly speed up simulations and analysis. This
 * file provides a generic `sampler_t` structure and factory functions for
 * creating different types of samplers (e.g., spatial, temporal).
 */

#pragma once

#include "libCacheSim/request.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sampler;
struct request;

/**
 * @brief Function pointer that determines if a request should be sampled.
 * @param sampler The sampler instance.
 * @param req The request to consider.
 * @return True if the request is sampled (i.e., should be included), false otherwise.
 */
typedef bool (*trace_sampling_func)(struct sampler *sampler, request_t *req);

/** @brief Function pointer to clone a sampler instance. */
typedef struct sampler *(*clone_sampler_func)(const struct sampler *sampler);

/** @brief Function pointer to free a sampler instance. */
typedef void (*free_sampler_func)(struct sampler *sampler);

/**
 * @brief Enumerates the different types of supported samplers.
 */
enum sampler_type {
  SPATIAL_SAMPLER,    /**< Samples based on object ID hash. */
  TEMPORAL_SAMPLER,   /**< Samples every Nth request. */
  SHARDS_SAMPLER,     /**< A sampling technique used by the SHARDS algorithm. */
  INVALID_SAMPLER
};

/**
 * @brief String representations for the sampler_type enum.
 */
static const char *const sampling_type_str[] = {"spatial", "temporal", "shards",
                                                "invalid"};

/**
 * @brief The main structure for a trace sampler.
 */
typedef struct sampler {
  trace_sampling_func sample;   /**< The function that implements the sampling logic. */
  int sampling_ratio_inv;       /**< The inverse of the sampling ratio (e.g., 100 for a 1% ratio). */
  double sampling_ratio;        /**< The target sampling ratio (e.g., 0.01 for 1%). */
  int sampling_salt;            /**< A salt used in hash-based sampling to get different samples. */
  void *other_params;           /**< A pointer to algorithm-specific parameters. */
  clone_sampler_func clone;     /**< Function to clone the sampler. */
  free_sampler_func free;       /**< Function to free the sampler. */
  enum sampler_type type;       /**< The type of the sampler. */
} sampler_t;

/**
 * @brief Creates a spatial sampler.
 *
 * Spatial sampling decides whether to sample a request based on a hash of its
 * object ID. All requests for a given object are either sampled or not.
 *
 * @param sampling_ratio The desired sampling ratio (e.g., 0.01 for 1%).
 * @return A pointer to the newly created sampler.
 */
sampler_t *create_spatial_sampler(double sampling_ratio);

/**
 * @brief Sets the salt for a spatial sampler.
 *
 * Using a different salt will result in a different, independent sample of objects.
 *
 * @param sampler The spatial sampler instance.
 * @param salt The new salt value to use.
 */
void set_spatial_sampler_salt(sampler_t *sampler, uint64_t salt);

/**
 * @brief Creates a temporal sampler.
 *
 * Temporal sampling simply samples every Nth request from the trace.
 *
 * @param sampling_ratio The desired sampling ratio (e.g., 0.1 for 10%).
 * @return A pointer to the newly created sampler.
 */
sampler_t *create_temporal_sampler(double sampling_ratio);

/**
 * @brief Prints information about a sampler for debugging.
 * @param sampler The sampler to print.
 */
static inline void print_sampler(sampler_t *sampler) {
  printf("%s sampler: sample ratio %lf\n", sampling_type_str[sampler->type],
         sampler->sampling_ratio);
}

/**
 * @brief Creates a SHARDS sampler.
 *
 * This is a specific sampling technique used in the SHARDS MRC profiling algorithm.
 *
 * @param sampling_ratio The desired sampling ratio.
 * @return A pointer to the newly created sampler.
 */
sampler_t *create_SHARDS_sampler(double sampling_ratio);

#ifdef __cplusplus
}
#endif
