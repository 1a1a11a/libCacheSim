#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <assert.h>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "../../include/libCacheSim/admissionAlgo.h"
#include "../../utils/include/mymath.h"

#define MAX_MODULE 10000000

typedef struct adaptsize_obj_info{
  double obj_seen_times;
  uint64_t obj_size;
} adaptsize_obj_info_t;

typedef struct adaptsize_admissioner {
  uint64_t cache_size;
  uint64_t max_iteration;
  uint64_t reconf_interval;
  uint64_t next_reconf;
  uint64_t stat_size;
  double c_param;
  double gss_v;
  std::unordered_map<obj_id_t, adaptsize_obj_info_t> interval_metadata;
  std::unordered_map<obj_id_t, adaptsize_obj_info_t> longterm_metadata;
  std::vector<double> aligned_obj_size;
  std::vector<double> aligned_obj_seen_times;
  std::vector<double> aligned_admission_probs;
} adaptsize_admission_params_t;

void adaptsize_update_stats(admissioner_t *admissioner, const request_t *req, const uint64_t cache_size);
void adaptsize_reconfiguration(adaptsize_admission_params_t *pa);
double adaptsize_model_hit_rate(adaptsize_admission_params_t *pa,double c);

bool adaptsize_admit(admissioner_t *admissioner, const request_t *req);
void free_adaptsize_admissioner(admissioner_t *admissioner);
admissioner_t *clone_adaptsize_admissioner(admissioner_t *admissioner);

// Constants from the original implementation
const double EWMA_DECAY = 0.3;
const double gss_r = 0.61803399;
const double tol = 3.0e-8;

//Math Formulas from the original implementation
static inline double adaptsize_oP1(double T, double l, double p) {
    return (l * p * T * (840.0 + 60.0 * l * T + 20.0 * l*l * T*T + l*l*l * T*T*T));
}
static inline double adaptsize_oP2(double T, double l, double p) {
    return (840.0 + 120.0 * l * (-3.0 + 7.0 * p) * T + 60.0 * l*l * (1.0 + p) * T*T + 4.0 * l*l*l * (-1.0 + 5.0 * p) * T*T*T + l*l*l*l * p * T*T*T*T);
}

void adaptsize_update_stats(admissioner_t *admissioner, const request_t *req, const uint64_t cache_size) {
  adaptsize_admission_params_t *pa = (adaptsize_admission_params_t *)admissioner->params;
  pa->cache_size = cache_size;

  adaptsize_reconfiguration(pa);

  if (pa->interval_metadata.count(req->obj_id) == 0 && pa->longterm_metadata.count(req->obj_id) == 0) {
    pa->stat_size += req->obj_size;
  } else {
    if (pa->interval_metadata.count(req->obj_id) > 0 && pa->interval_metadata[req->obj_id].obj_size != req->obj_size){
      pa->stat_size -= pa->interval_metadata[req->obj_id].obj_size;
      pa->stat_size += req->obj_size;
    }
    if (pa->longterm_metadata.count(req->obj_id) > 0 && pa->longterm_metadata[req->obj_id].obj_size != req->obj_size){
      pa->stat_size -= pa->longterm_metadata[req->obj_id].obj_size;
      pa->stat_size += req->obj_size;
    }
  }

  auto& obj_info = pa->interval_metadata[req->obj_id];
  obj_info.obj_seen_times += 1.0;
  obj_info.obj_size = req->obj_size;
}

void adaptsize_reconfiguration(adaptsize_admission_params_t *pa) {
  // Check if its time to reconfigure
  --pa->next_reconf;
  if (pa->next_reconf > 0) {
    return;
  } else if (pa->stat_size <= pa->cache_size * 3) {
    pa->next_reconf += 10000;
    return;
  } else {
    pa->next_reconf = pa->reconf_interval;
  }
  // END Check if its time to reconfigure
  // Reconfiguration Initialization
  for (auto it = pa->longterm_metadata.begin(); it != pa->longterm_metadata.end(); it++) {
    it->second.obj_seen_times *= EWMA_DECAY;
  }

  for (auto it = pa->interval_metadata.begin(); it != pa->interval_metadata.end(); it++) {
    auto lt_metadata = pa->longterm_metadata.find(it->first) ;
    if (lt_metadata != pa->longterm_metadata.end()) {
      lt_metadata->second.obj_seen_times += (1. - EWMA_DECAY) * it->second.obj_seen_times;
      lt_metadata->second.obj_size = it->second.obj_size;
    } else {
      pa->longterm_metadata.insert(*it);
    }
  }
  pa->interval_metadata.clear();

  pa->aligned_obj_seen_times.clear();
  pa->aligned_obj_size.clear();

  double total_seen_times = 0.0;
  uint64_t total_obj_size = 0.0;

  for (auto it = pa->longterm_metadata.begin(); it != pa->longterm_metadata.end(); it++) {
    if (it->second.obj_seen_times < 0.1) {
      pa->stat_size -= it->second.obj_size;
      it = pa->longterm_metadata.erase(it);
    } else {
      pa->aligned_obj_seen_times.push_back(it->second.obj_seen_times);
      total_seen_times += it->second.obj_seen_times;
      pa->aligned_obj_size.push_back(it->second.obj_size);
      total_obj_size += it->second.obj_size;
    }
  }
  VVERBOSE("Reconfiguring over %zu objects - log2 total size %f log2 statsize %f\n", 
    pa->longterm_metadata.size(), 
    log2(total_obj_size), 
    log2(pa->stat_size));
  // END Reconfiguration Initialization
  // MATH
  double x0 = 0;
  double x1 = log2(pa->cache_size);
  double x2 = x1;
  double x3 = x1;

  double best_hit_rate = 0.0;
  for (int i = 2; i < x3; i += 4) {
    const double next_log2c = i;
    const double hit_rate = adaptsize_model_hit_rate(pa, next_log2c);
    if (hit_rate > best_hit_rate) {
      best_hit_rate = hit_rate;
      x1 = next_log2c;
    }
  }

  double h1 = best_hit_rate;
  double h2 = 0.0;

  if (x3 - x1 > x1 - x0) {
    x2 = x1 + pa->gss_v * (x3 - x1);
    h2 = adaptsize_model_hit_rate(pa, x2);
  } else {
    x2 = x1;
    h2 = h1;
    x1 = x0 + pa->gss_v * (x1-x0);
    h1 = adaptsize_model_hit_rate(pa, x1);
  }
  assert(x1 < x2);
  uint64_t current_iteration = 0;
  while (current_iteration++ < pa->max_iteration 
    && fabs(x3 - x0) > tol * (fabs(x1) + fabs(x2))) {
      if (h1 != h1 || h2 != h2) {
        //Error NaN
        WARN("BUG: 1 NaN h1:%f h2:%f\n", h1, h2);
        break;
      }
      if (h2 > h1) {
        x0 = x1;
        x1 = x2;
        x2 = gss_r * x1 + pa->gss_v * x3;
        h1 = h2;
        h2 = adaptsize_model_hit_rate(pa, x2);
      } else {
        x3 = x2;
        x2 = x1;
        x1 = gss_r * x2 + pa->gss_v * x0;
        h2 = h1;
        h1 = adaptsize_model_hit_rate(pa, x1);
      }
    }
    // END MATH
    // Check for result
    if (h1 != h1 || h2 != h2) {
      //Error NaN
      WARN("BUG: 2 NaN h1:%f h2:%f\n", h1, h2);
    } else if (h1 > h2) {
      pa->c_param = pow(2, x1);
      VVERBOSE("C = %f (log2: %f )\n", pa->c_param, x1);
    } else {
      pa->c_param = pow(2, x2);
      VVERBOSE("C = %f (log2: %f )\n", pa->c_param, x2);
    }
    // END Check for result
}

double adaptsize_model_hit_rate(adaptsize_admission_params_t* pa,double log2c){
  double old_T, the_T, the_C;
  double sum_val = 0.;
  double thparam = log2c;

  for (size_t i = 0; i < pa->aligned_obj_seen_times.size(); i++) {
    sum_val += pa->aligned_obj_seen_times[i] * (exp(-pa->aligned_obj_size[i]/ pow(2,thparam))) * pa->aligned_obj_size[i];
  }
  if(sum_val <= 0) {
    return(0);
  }
  the_T = pa->cache_size / sum_val;
  // prepare admission probabilities
  pa->aligned_admission_probs.clear();
  for(size_t i = 0; i < pa->aligned_obj_seen_times.size(); i++) {
    pa->aligned_admission_probs.push_back(exp(-pa->aligned_obj_size[i] / pow(2.0, thparam)));
  }
  // 20 iterations to calculate TTL
  for(int j = 0; j < 20; j++) {
    the_C = 0;
    if(the_T > 1e70) {
      break;
    }
    for(size_t i = 0; i < pa->aligned_obj_seen_times.size(); i++) {
      const double reqTProd = pa->aligned_obj_seen_times[i] * the_T;
      if(reqTProd > 150) {
        // cache hit probability = 1, but numerically inaccurate to calculate
        the_C += pa->aligned_obj_size[i];
      } else {
        const double expTerm = exp(reqTProd) - 1;
        const double expAdmProd = pa->aligned_admission_probs[i] * expTerm;
        const double tmp = expAdmProd / (1 + expAdmProd);
        the_C += pa->aligned_obj_size[i] * tmp;
      }
    }
    old_T = the_T;
    the_T = pa->cache_size * old_T/the_C;
  }
  assert(the_C > 0);

  // calculate object hit ratio
  double weighted_hitratio_sum = 0;
  for(size_t i = 0; i < pa->aligned_obj_seen_times.size(); i++) {
    const double tmp01= adaptsize_oP1(the_T,pa->aligned_obj_seen_times[i],pa->aligned_admission_probs[i]);
    const double tmp02= adaptsize_oP2(the_T,pa->aligned_obj_seen_times[i],pa->aligned_admission_probs[i]);
    double tmp;
    if(tmp01 != 0 && tmp02 == 0)
      tmp = 0.0;
    else 
      tmp= tmp01 / tmp02;
    if(tmp < 0.0)
      tmp = 0.0;
    else if (tmp > 1.0)
      tmp = 1.0;
    weighted_hitratio_sum += pa->aligned_obj_seen_times[i] * tmp;
  }
  return weighted_hitratio_sum;
}

bool adaptsize_admit(admissioner_t *admissioner, const request_t *req) {
  adaptsize_admission_params_t *pa =
      (adaptsize_admission_params_t *)admissioner->params;
  double prob = exp(-(double)req->obj_size/pa->c_param);
  if ((double)(next_rand() % MAX_MODULE) / (double)MAX_MODULE < prob) {
    return true;
  }
  return false;
}

static void adaptsize_admissioner_parse_params(
    const char *init_params, adaptsize_admission_params_t *pa) {
  if (init_params != NULL) {
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

      if (strcasecmp(key, "max-iteration") == 0) {
        pa->max_iteration = strtoll(value, &end, 10);
      } else if (strcasecmp(key, "c-param-shift") == 0) {
        pa->c_param = 1 << atoi(value);
      } else if (strcasecmp(key, "reconf-interval") == 0) {
        pa->reconf_interval = strtoull(value, &end, 10);
      } else if (strcasecmp(key, "print") == 0){
        printf("max-iteration=%lu,c-param-shift=%f,reconf-interval=%lu", pa->max_iteration, pa->c_param, pa->reconf_interval);
        exit(0);
      } else {
        ERROR("adaptsize admission does not have parameter %s\n", key);
        exit(1);
      }
    }
    free(old_params_str);
  }
}

admissioner_t *clone_adaptsize_admissioner(admissioner_t *admissioner) {
  return create_adaptsize_admissioner((const char *)admissioner->init_params);
}

void free_adaptsize_admissioner(admissioner_t *admissioner) {
  adaptsize_admission_params_t *pa = 
    (adaptsize_admission_params_t*)(admissioner->params);
  
  free(pa);

  if (admissioner->init_params) {
    free(admissioner->init_params);
  }
  free(admissioner);
}

admissioner_t *create_adaptsize_admissioner(const char *init_params) {
  adaptsize_admission_params_t *pa = (adaptsize_admission_params_t *)malloc(
      sizeof(adaptsize_admission_params_t));
  memset(pa, 0, sizeof(adaptsize_admission_params_t));

  pa->reconf_interval = 15000;
  pa->max_iteration = 15;
  pa->c_param = 1 << 15;

  adaptsize_admissioner_parse_params(init_params, pa);

  admissioner_t *admissioner = (admissioner_t *)malloc(sizeof(admissioner_t));
  memset(admissioner, 0, sizeof(admissioner_t));

  pa->interval_metadata = std::unordered_map<obj_id_t, adaptsize_obj_info_t>{};
  pa->longterm_metadata = std::unordered_map<obj_id_t, adaptsize_obj_info_t>{};

  pa->aligned_admission_probs = std::vector<double>{};
  pa->aligned_obj_seen_times = std::vector<double>{};
  pa->aligned_obj_size = std::vector<double>{};

  pa->gss_v = 1 - gss_r;
  pa->stat_size = 0;
  pa->c_param = 1 << 15;
  pa->next_reconf = pa->reconf_interval;

  admissioner->params = pa;
  admissioner->admit = adaptsize_admit;
  admissioner->free = free_adaptsize_admissioner;
  admissioner->clone = clone_adaptsize_admissioner;
  admissioner->update = adaptsize_update_stats;
  if (init_params != NULL) admissioner->init_params = strdup(init_params);

  return admissioner;
}