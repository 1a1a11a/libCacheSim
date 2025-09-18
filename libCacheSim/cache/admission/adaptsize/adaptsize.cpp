/**
 * @file adaptsize.cpp
 * @brief Implements the C++ class for the AdaptSize admission algorithm.
 */

#include "adaptsize.h"

#include <sys/types.h>

#include <cmath>
#include <cstdint>

#include "utils/include/mymath.h"

#define MAX_MODULE 10000000

// Constants used in the original implementation
const double EWMA_DECAY = 0.3;
const double gss_r = 0.61803399; // Golden section search ratio
const double tol = 3.0e-8;

/**
 * @brief Constructs an Adaptsize admission controller.
 * @param max_iteration_param The maximum number of iterations for the optimization search.
 * @param reconf_interval_param The number of requests between reconfigurations.
 */
Adaptsize::Adaptsize(const uint64_t max_iteration_param,
                     const uint64_t reconf_interval_param)
    : cache_size(0),
      max_iteration(max_iteration_param),
      reconf_interval(reconf_interval_param),
      next_reconf(reconf_interval_param),
      stat_size(0),
      c_param(1 << 15),
      gss_v(1 - gss_r) {}

/**
 * @brief Copy constructor.
 * @param other The Adaptsize object to copy from.
 */
Adaptsize::Adaptsize(const Adaptsize& other) = default;

/**
 * @brief Move constructor.
 * @param other The Adaptsize object to move from.
 */
Adaptsize::Adaptsize(Adaptsize&& other) noexcept = default;

/**
 * @brief Copy assignment operator.
 * @param other The Adaptsize object to copy from.
 * @return Reference to this object.
 */
Adaptsize& Adaptsize::operator=(const Adaptsize& other) = default;

/**
 * @brief Move assignment operator.
 * @param other The Adaptsize object to move from.
 * @return Reference to this object.
 */
Adaptsize& Adaptsize::operator=(Adaptsize&& other) noexcept = default;

/**
 * @brief Updates statistics based on a new request and triggers reconfiguration if needed.
 * @param req The request being processed.
 * @param cache_size_param The current size of the cache.
 */
void Adaptsize::updateStats(const request_t* req,
                            const uint64_t cache_size_param) {
  this->cache_size = cache_size_param;
  reconfigure();

  // Update statistics for the current interval
  if (interval_metadata.find(req->obj_id) == interval_metadata.end()) {
      stat_size += req->obj_size;
  } else if (interval_metadata[req->obj_id].obj_size != req->obj_size) {
      stat_size -= interval_metadata[req->obj_id].obj_size;
      stat_size += req->obj_size;
  }
  auto& oinfo = interval_metadata[req->obj_id];
  oinfo.obj_seen_times += 1.0;
  oinfo.obj_size = req->obj_size;
}

/**
 * @brief Reconfigures the admission parameter `c_param` by modeling the hit rate.
 *
 * This is the core of the AdaptSize algorithm. It is called periodically.
 * It merges statistics from the last interval into a long-term view, uses
 * Golden Section Search to find the optimal `c_param` that maximizes the
 * modeled hit rate, and updates the `c_param` for the next interval.
 */
void Adaptsize::reconfigure() {
  if (--next_reconf > 0) {
    return;
  }
  next_reconf = reconf_interval;

  if (stat_size <= cache_size * 3) {
    return; // Not enough new data to justify a reconfiguration
  }

  // Merge interval stats into long-term stats using an exponential moving average
  for (auto& obj : longterm_metadata) {
    obj.second.obj_seen_times *= EWMA_DECAY;
  }
  for (auto& obj : interval_metadata) {
    if (longterm_metadata.find(obj.first) == longterm_metadata.end()) {
      longterm_metadata[obj.first] = obj.second;
    } else {
      longterm_metadata[obj.first].obj_seen_times += (1 - EWMA_DECAY) * obj.second.obj_seen_times;
      longterm_metadata[obj.first].obj_size = obj.second.obj_size;
    }
  }
  interval_metadata.clear();

  // Prepare stats for modeling
  aligned_obj_seen_times.clear();
  aligned_obj_size.clear();
  for (auto it = longterm_metadata.begin(); it != longterm_metadata.end();) {
    if (it->second.obj_seen_times < 0.1) {
      stat_size -= it->second.obj_size;
      it = longterm_metadata.erase(it);
    } else {
      aligned_obj_seen_times.push_back(it->second.obj_seen_times);
      aligned_obj_size.push_back(it->second.obj_size);
      ++it;
    }
  }

  // Find the optimal C value using Golden Section Search
  double x0 = 0, x3 = log2(cache_size), x1 = x3, x2 = x3;
  double best_hit_rate = 0.0;

  // Initial rough search for a good starting point
  for (int i = 2; i < x3; i += 4) {
    const double hit_rate = modelHitRate(i);
    if (hit_rate > best_hit_rate) {
      best_hit_rate = hit_rate;
      x1 = i;
    }
  }

  double h1 = best_hit_rate, h2 = 0.0;
  if (x3 - x1 > x1 - x0) {
    x2 = x1 + gss_v * (x3 - x1);
    h2 = modelHitRate(x2);
  } else {
    x2 = x1;
    h2 = h1;
    x1 = x0 + gss_v * (x1 - x0);
    h1 = modelHitRate(x1);
  }

  // Golden Section Search main loop
  for (uint64_t current_iteration = 0;
       current_iteration < max_iteration && fabs(x3 - x0) > tol * (fabs(x1) + fabs(x2));
       ++current_iteration) {
    if (std::isnan(h1) || std::isnan(h2)) break;
    if (h2 > h1) {
      x0 = x1; x1 = x2; x2 = gss_r * x1 + gss_v * x3;
      h1 = h2; h2 = modelHitRate(x2);
    } else {
      x3 = x2; x2 = x1; x1 = gss_r * x2 + gss_v * x0;
      h2 = h1; h1 = modelHitRate(x1);
    }
  }

  // Set the new c_param based on the search result
  if (std::isnan(h1) || std::isnan(h2)) {
    WARN("BUG: NaN in Golden Section Search h1:%f h2:%f\n", h1, h2);
  } else {
    c_param = pow(2, (h1 > h2) ? x1 : x2);
  }
}

/**
 * @brief Decides whether to admit an object based on its size and the current `c_param`.
 *
 * The admission probability is calculated as `exp(-object_size / c_param)`.
 *
 * @param req The request to consider.
 * @return True if the object should be admitted, false otherwise.
 */
bool Adaptsize::admit(const request_t* req) {
  double prob = exp(-req->obj_size / c_param);
  double roll = (double)(next_rand() % MAX_MODULE) / (double)MAX_MODULE;
  return roll < prob;
}

// Mathematical formulas used in the hit rate model, based on the original paper.
static inline double oP1(double T, double l, double p) {
  return (l * p * T * (840.0 + 60.0 * l * T + 20.0 * l * l * T * T + l * l * l * T * T * T));
}
static inline double oP2(double T, double l, double p) {
  return (840.0 + 120.0 * l * (-3.0 + 7.0 * p) * T +
          60.0 * l * l * (1.0 + p) * T * T +
          4.0 * l * l * l * (-1.0 + 5.0 * p) * T * T * T +
          l * l * l * l * p * T * T * T * T);
}

/**
 * @brief Models the expected hit rate for a given cache size parameter.
 *
 * This function implements the mathematical model from the AdaptSize paper to
 * predict the cache hit rate given the current workload statistics and a
 * potential `c_param` value (represented as `log2c`).
 *
 * @param log2c The log-base-2 of the `c_param` to model.
 * @return The predicted hit rate as a double.
 */
double Adaptsize::modelHitRate(double log2c) {
  double old_T, the_T, the_C;
  double sum_val = 0.;
  double thparam = pow(2.0, log2c);

  for (size_t i = 0; i < aligned_obj_seen_times.size(); i++) {
    sum_val += aligned_obj_seen_times[i] *
               (exp(-aligned_obj_size[i] / thparam)) *
               aligned_obj_size[i];
  }
  if (sum_val <= 0) return 0.0;

  the_T = cache_size / sum_val;
  aligned_admission_probs.assign(aligned_obj_seen_times.size(), 0.0);
  for (size_t i = 0; i < aligned_obj_seen_times.size(); i++) {
    aligned_admission_probs[i] = exp(-aligned_obj_size[i] / thparam);
  }

  // Iteratively solve for the characteristic time T
  for (int j = 0; j < 20; j++) {
    the_C = 0;
    if (the_T > 1e70) break;
    for (size_t i = 0; i < aligned_obj_seen_times.size(); i++) {
      const double reqTProd = aligned_obj_seen_times[i] * the_T;
      if (reqTProd > 150) {
        the_C += aligned_obj_size[i];
      } else {
        const double expTerm = exp(reqTProd) - 1.0;
        const double expAdmProd = aligned_admission_probs[i] * expTerm;
        the_C += aligned_obj_size[i] * (expAdmProd / (1.0 + expAdmProd));
      }
    }
    old_T = the_T;
    the_T = cache_size * old_T / the_C;
  }

  // Calculate the final weighted hit rate
  double weighted_hitratio_sum = 0;
  for (size_t i = 0; i < aligned_obj_seen_times.size(); i++) {
    const double tmp01 = oP1(the_T, aligned_obj_seen_times[i], aligned_admission_probs[i]);
    const double tmp02 = oP2(the_T, aligned_obj_seen_times[i], aligned_admission_probs[i]);
    double tmp = (tmp02 != 0) ? (tmp01 / tmp02) : 0.0;
    tmp = std::max(0.0, std::min(1.0, tmp));
    weighted_hitratio_sum += aligned_obj_seen_times[i] * tmp;
  }
  return weighted_hitratio_sum;
}
