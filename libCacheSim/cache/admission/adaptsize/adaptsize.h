/**
 * @file adaptsize.h
 * @brief Defines the C++ class for the AdaptSize admission algorithm.
 *
 * AdaptSize is a sophisticated admission policy that periodically analyzes
 * access statistics to model the cache's hit rate. It then uses this model
 * to dynamically adjust its admission policy, aiming to maximize the hit rate
 * for the given workload and cache size.
 *
 * Based on the paper: "AdaptSize: Orchestrating the Hot Object Memory Cache
 * in a Content Delivery Network" by F. Poese, et al.
 * https://dl.acm.org/doi/10.1145/2068816.2068819
 */

#ifndef LIBCACHESIM_ADMISSION_ADAPTSIZE_H
#define LIBCACHESIM_ADMISSION_ADAPTSIZE_H

#include <sys/types.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "libCacheSim/request.h"

class Adaptsize {
 public:
  /**
   * @brief Constructs an Adaptsize admission controller.
   * @param max_iteration Not currently used.
   * @param reconf_interval The number of requests between reconfigurations.
   */
  Adaptsize(const uint64_t max_iteration, const uint64_t reconf_interval);

  // Copy constructor
  Adaptsize(const Adaptsize& other);

  // Move constructor
  Adaptsize(Adaptsize&& other) noexcept;

  // Copy assignment operator
  Adaptsize& operator=(const Adaptsize& other);

  // Move assignment operator
  Adaptsize& operator=(Adaptsize&& other) noexcept;

  /**
   * @brief Decides whether to admit a request based on the current policy.
   * @param req The request to consider for admission.
   * @return True to admit the object, false otherwise.
   */
  bool admit(const request_t* req);

  /**
   * @brief Updates the internal statistics with a new request.
   *
   * This function is called for every request and collects statistics.
   * Periodically, it will trigger the `reconfigure` method.
   *
   * @param req The request to process.
   * @param cache_size The current size of the cache.
   */
  void updateStats(const request_t* req, const uint64_t cache_size);

 private:
  /**
   * @brief Reconfigures the admission policy based on collected stats.
   *
   * This method analyzes the statistics gathered during the last interval,
   * rebuilds the hit rate model, and updates the admission policy for the
   * next interval.
   */
  void reconfigure();

  /**
   * @brief Models the hit rate for a given cache size.
   * @param log2c The log-base-2 of the cache size.
   * @return The estimated hit rate.
   */
  double modelHitRate(double log2c);

  uint64_t cache_size;
  uint64_t max_iteration;
  uint64_t reconf_interval;
  uint64_t next_reconf;
  uint64_t stat_size;
  double c_param; // The 'c' parameter from the paper, determining admission probability.
  double gss_v;   // Golden section search variable.

  struct obj_info {
    double obj_seen_times;
    int64_t obj_size;
  };

  // Maps for tracking object stats within an interval and long-term.
  std::unordered_map<obj_id_t, obj_info> interval_metadata;
  std::unordered_map<obj_id_t, obj_info> longterm_metadata;

  // Vectors used during the reconfiguration process.
  std::vector<double> aligned_obj_size;
  std::vector<double> aligned_obj_seen_times;
  std::vector<double> aligned_admission_probs;
};

#endif  // LIBCACHESIM_ADMISSION_ADAPTSIZE_H
