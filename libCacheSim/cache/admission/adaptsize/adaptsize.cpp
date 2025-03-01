#include "adaptsize.h"
#include <sys/types.h>
#include <cmath>
#include <cstdint>
#include "../../utils/include/mymath.h"

#define MAX_MODULE 10000000

// Const used in original implementation
const double EWMA_DECAY = 0.3;
const double gss_r = 0.61803399;
const double tol = 3.0e-8;

/**
 * @brief Initialzie Adaptstat
 * @param max_iteration
 * @param reconf_interval
 */
Adaptsize::Adaptsize(const uint64_t max_iteration, const uint64_t reconf_interval)
  : max_iteration(max_iteration),
  reconf_interval(reconf_interval),
  gss_v(1 - gss_r),
  c_param(1 << 15),
  stat_size(0),
  next_reconf(reconf_interval),
  cache_size(0)
{
}

/**
 * @brief This function get called for every lookup to update adaptsize stats
 * @param req
 * @param cache_size current cache size
 */
void Adaptsize::updateStats(const request_t *req, const uint64_t cache_size) {
  this->cache_size = cache_size;
  reconfigure();
  if (interval_metadata.count(req->obj_id) == 0
    && longterm_metadata.count(req->obj_id) == 0) {
    stat_size += req->obj_size;
  } else {
    if (interval_metadata.count(req->obj_id) > 0
      && interval_metadata[req->obj_id].obj_size != req->obj_size) {
      stat_size -= interval_metadata[req->obj_id].obj_size;
      stat_size += req->obj_size;
    }
    if (longterm_metadata.count(req->obj_id) > 0
      && longterm_metadata[req->obj_id].obj_size != req->obj_size) {
      stat_size -= longterm_metadata[req->obj_id].obj_size;
      stat_size += req->obj_size;
    }
  }
  auto& obj_info = interval_metadata[req->obj_id];
  obj_info.obj_seen_times += 1.0;
  obj_info.obj_size = req->obj_size;
}

/**
 * @brief This function get called before updating stats. Used to get the best C for the interval
 */
void Adaptsize::reconfigure(){
  // Check if its time for reconfiguration
  --next_reconf;
  if (next_reconf > 0) {
    return;
  }
  if (stat_size <= cache_size * 3) {
    next_reconf += 1000;
    return;
  }
  // END Check if its time for reconfiguration
  // Prepare for reconf
  next_reconf = reconf_interval;
  for (auto& obj : longterm_metadata) {
    obj.second.obj_seen_times *= EWMA_DECAY;
  }
  for (auto& obj : interval_metadata) {
    if (longterm_metadata.count(obj.first) == 0) {
      longterm_metadata[obj.first] = obj.second;
      continue;
    }
    longterm_metadata[obj.first].obj_seen_times += (1 - EWMA_DECAY) * obj.second.obj_seen_times;
    longterm_metadata[obj.first].obj_size = obj.second.obj_size;
  }
  interval_metadata.clear();
  aligned_obj_seen_times.clear();
  aligned_obj_size.clear();

  double total_seen_times = 0.0;
  uint64_t total_obj_size = 0.0;

  for (auto it = longterm_metadata.begin();
    it != longterm_metadata.end();
  ) {
    if (it->second.obj_seen_times < 0.1) {
      stat_size -= it->second.obj_size;
      it = longterm_metadata.erase(it);
      continue;
    }
    aligned_obj_seen_times.push_back(it->second.obj_seen_times);
    total_seen_times += it->second.obj_seen_times;
    aligned_obj_size.push_back(it->second.obj_size);
    total_obj_size += it->second.obj_size;
    ++it;
  }
  VVERBOSE("Reconfiguring over %zu objects - log2 total size %f log2 statsize %f\n",
    longterm_metadata.size(),
    log2(total_obj_size),
    log2(stat_size)
  );
  // END Prepare for reconf
  // Finding the value of C with the best hit rate
  double x0 = 0;
  double x1 = log2(cache_size);
  double x2 = x1;
  double x3 = x1;

  double best_hit_rate = 0.0;
  for (int i = 2; i < x3; i += 4) {
    const double next_log2c = i;
    const double hit_rate = modelHitRate(next_log2c);
    if (hit_rate > best_hit_rate) {
      best_hit_rate = hit_rate;
      x1 = next_log2c;
    }
  }

  double h1 = best_hit_rate;
  double h2 = 0.0;

  if (x3 - x1 > x1 - x0) {
    x2 = x1 + gss_v * (x3 - x1);
    h2 = modelHitRate( x2);
  } else {
    x2 = x1;
    h2 = h1;
    x1 = x0 + gss_v * (x1-x0);
    h1 = modelHitRate(x1);
  }
  uint64_t current_iteration = 0;
  while (current_iteration++ < max_iteration
    && fabs(x3 - x0) > tol * (fabs(x1) + fabs(x2))
  ) {
    if (h1 != h1 || h2 != h2) {
      //Error NaN
      WARN("BUG: NaN h1:%f h2:%f\n", h1, h2);
      break;
    }
    if (h2 > h1) {
      x0 = x1;
      x1 = x2;
      x2 = gss_r * x1 + gss_v * x3;
      h1 = h2;
      h2 = modelHitRate(x2);
    } else {
      x3 = x2;
      x2 = x1;
      x1 = gss_r * x2 + gss_v * x0;
      h2 = h1;
      h1 = modelHitRate(x1);
    }
    }
  // END Finding the value of C with the best hit rate
  // Check for result
  if (h1 != h1 || h2 != h2) {
    //Error NaN
    WARN("BUG: NaN h1:%f h2:%f\n", h1, h2);
  } else if (h1 > h2) {
    c_param = pow(2, x1);
    VVERBOSE("C = %f (log2: %f )\n", c_param, x1);
  } else {
    c_param = pow(2, x2);
    VVERBOSE("C = %f (log2: %f )\n", c_param, x2);
  }
  // END Check for result
}

/**
 * @brief This function get called before admitting object. Using modified size probability with C param
 * @param req
 * @return true / false
 */
bool Adaptsize::admit(const request_t *req) {
  double prob = exp( -req->obj_size / c_param);
  double roll = (double)(next_rand() % MAX_MODULE) / (double)MAX_MODULE;
  return roll < prob;
}

// Math formula used in original implementation
static inline double oP1(double T, double l, double p) {
  return (l * p * T * (840.0 + 60.0 * l * T + 20.0 * l*l * T*T + l*l*l * T*T*T));
}
static inline double oP2(double T, double l, double p) {
  return (840.0 + 120.0 * l * (-3.0 + 7.0 * p) * T + 60.0 * l*l * (1.0 + p) * T*T + 4.0 * l*l*l * (-1.0 + 5.0 * p) * T*T*T + l*l*l*l * p * T*T*T*T);
}

/**
 * @brief This function get called a lot in reconfigure function, used to predict C hit rate
 * @param log2c
 * @return hit rate prediction
 */
double Adaptsize::modelHitRate(double log2c) {
  double old_T, the_T, the_C;
  double sum_val = 0.;
  double thparam = log2c;

  for (size_t i = 0; i < aligned_obj_seen_times.size(); i++) {
    sum_val += aligned_obj_seen_times[i] * (exp(-aligned_obj_size[i]/ pow(2,thparam))) * aligned_obj_size[i];
  }
  if(sum_val <= 0) {
    return(0);
  }
  the_T = cache_size / sum_val;
  aligned_admission_probs.clear();
  for(size_t i = 0; i < aligned_obj_seen_times.size(); i++) {
    aligned_admission_probs.push_back(exp(-aligned_obj_size[i] / pow(2.0, thparam)));
  }
  for(int j = 0; j < 20; j++) {
    the_C = 0;
    if(the_T > 1e70) {
      break;
    }
    for(size_t i = 0; i < aligned_obj_seen_times.size(); i++) {
      const double reqTProd = aligned_obj_seen_times[i] * the_T;
      if(reqTProd > 150) {
        the_C += aligned_obj_size[i];
      } else {
        const double expTerm = exp(reqTProd) - 1;
        const double expAdmProd = aligned_admission_probs[i] * expTerm;
        const double tmp = expAdmProd / (1 + expAdmProd);
        the_C += aligned_obj_size[i] * tmp;
      }
    }
    old_T = the_T;
    the_T = cache_size * old_T/the_C;
  }

  double weighted_hitratio_sum = 0;
  for(size_t i = 0; i < aligned_obj_seen_times.size(); i++) {
    const double tmp01= oP1(the_T,aligned_obj_seen_times[i],aligned_admission_probs[i]);
    const double tmp02= oP2(the_T,aligned_obj_seen_times[i],aligned_admission_probs[i]);
    double tmp;
    if(tmp01 != 0 && tmp02 == 0)
      tmp = 0.0;
    else
      tmp= tmp01 / tmp02;
    if(tmp < 0.0)
      tmp = 0.0;
    else if (tmp > 1.0)
      tmp = 1.0;
    weighted_hitratio_sum += aligned_obj_seen_times[i] * tmp;
  }
  return weighted_hitratio_sum;
}