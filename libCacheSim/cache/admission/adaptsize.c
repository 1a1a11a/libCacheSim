#include <fcntl.h>
#include <math.h>
#include <glib.h>
#include <strings.h>
#include <sys/types.h>
#include <assert.h>
#include "../../include/libCacheSim/admissionAlgo.h"
#include "../../utils/include/mymath.h"

#ifdef __cplusplus
extern "C" {
#endif

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
  GHashTable *interval_metadata;
  GHashTable *longterm_metadata;
  GArray *aligned_obj_size;
  GArray *aligned_obj_seen_times;
  GArray *aligned_admission_probs;
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
  // INFO("START Update\n");
  adaptsize_admission_params_t *pa = (adaptsize_admission_params_t *)admissioner->params;
  pa->cache_size = cache_size;

  adaptsize_reconfiguration(pa);
  
  gpointer key = GINT_TO_POINTER(req->obj_id);

  adaptsize_obj_info_t* obj_info = (adaptsize_obj_info_t*)g_hash_table_lookup(pa->interval_metadata, key);
  if (obj_info) {
    obj_info->obj_seen_times += 1.0;
    if (obj_info->obj_size != req->obj_size) {
      pa->stat_size += req->obj_size - obj_info->obj_size;
      pa->stat_size -= obj_info->obj_size;
      obj_info->obj_size = req->obj_size;
    }
    return;
  } 
  adaptsize_obj_info_t* new_obj_info = g_try_new(adaptsize_obj_info_t, 1);
  if (new_obj_info == NULL) {
    ERROR("Memory allocation failed for new_obj_info\n");
  }
  new_obj_info->obj_seen_times = 1.0;
  new_obj_info->obj_size = req->obj_size;

  g_hash_table_insert(pa->interval_metadata, key, new_obj_info);
  pa->stat_size += req->obj_size; 
  // INFO("END Update\n");
}

void adaptsize_decay(gpointer key, gpointer value, gpointer user_data) {
  adaptsize_obj_info_t * obj_info = (adaptsize_obj_info_t*) value;
  obj_info->obj_seen_times *= EWMA_DECAY;
}

void adaptsize_reconfiguration(adaptsize_admission_params_t *pa) {
  // Check if its time to reconfigure
  pa->next_reconf--;
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
  g_hash_table_foreach(pa->longterm_metadata, adaptsize_decay, NULL);

  GHashTableIter iter;
  gpointer key, val;

  g_hash_table_iter_init(&iter,  pa->interval_metadata);
  while (g_hash_table_iter_next(&iter, &key, &val)) {
    adaptsize_obj_info_t* obj_info = (adaptsize_obj_info_t*) val; 
    adaptsize_obj_info_t* lt_metadata = (adaptsize_obj_info_t*) g_hash_table_lookup(pa->longterm_metadata, key);
    if (lt_metadata) {
      lt_metadata->obj_seen_times += (1. - EWMA_DECAY) * obj_info->obj_seen_times;
      lt_metadata->obj_size = obj_info->obj_size;
    } else {
      g_hash_table_insert(pa->longterm_metadata, key, val);
    }
  }
  g_hash_table_remove_all(pa->interval_metadata);

  g_array_set_size(pa->aligned_obj_size, 0);
  g_array_set_size(pa->aligned_obj_seen_times, 0);

  double total_seen_times;
  uint64_t total_obj_size;

  g_hash_table_iter_init(&iter,  pa->longterm_metadata);
  while (g_hash_table_iter_next(&iter, &key, &val)) {
    adaptsize_obj_info_t* obj_info = (adaptsize_obj_info_t*)val; 
    if (obj_info->obj_seen_times < 0.1) {
      pa->stat_size -= obj_info->obj_size;
      g_hash_table_iter_remove(&iter);
    } else {
      g_array_append_val(pa->aligned_obj_seen_times, obj_info->obj_seen_times);
      total_seen_times += obj_info->obj_seen_times;
      g_array_append_val(pa->aligned_obj_size, obj_info->obj_size);
      total_obj_size += obj_info->obj_size;
    }
  }
  INFO("Reconfiguring over %u objects - log2 total size %f log2 statsize %f\n", 
    g_hash_table_size(pa->longterm_metadata), 
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
  double h2;

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
        ERROR("ERROR: Numerical Error / NaN")
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
      ERROR("BUG: Numerical Error / NaN\n")
    } else if (h1 > h2) {
      pa->c_param = pow(2, x1);
      INFO("C = %f (log2: %f )\n", pa->c_param, x1);
    } else {
      pa->c_param = pow(2, x2);
      INFO("C = %f (log2: %f )\n", pa->c_param, x2);
    }
    // END Check for result
}

double adaptsize_model_hit_rate(adaptsize_admission_params_t* pa,double c){
  double old_T, the_T, the_C;
  double sum_val = 0.;
  double thparam = c;

  double seen_times;
  double obj_size;
  double adm_prob;

  for (size_t i = 0; i < pa->aligned_obj_seen_times->len; i++) {
    seen_times = g_array_index(pa->aligned_obj_seen_times, double, i);
    obj_size = g_array_index(pa->aligned_obj_size, uint64_t, i);
    sum_val += seen_times * (exp(-obj_size / pow(2, thparam))) * obj_size;
  }
  if(sum_val <= 0) {
    return(0);
  }
  the_T = pa->cache_size / sum_val;
  // prepare admission probabilities
  g_array_set_size(pa->aligned_admission_probs, 0);
  for(size_t i = 0; i < pa->aligned_obj_seen_times->len; i++) {
    seen_times = g_array_index(pa->aligned_obj_seen_times, double, i);
    obj_size = g_array_index(pa->aligned_obj_size, uint64_t, i);
    double admission_prob = exp(obj_size / pow(2.0, thparam));
    g_array_append_val(pa->aligned_admission_probs, admission_prob);
  }
  // 20 iterations to calculate TTL
  for(int j = 0; j < 10; j++) {
    the_C = 0;
    if(the_T > 1e70) {
      break;
    }
    for(size_t i = 0; i < pa->aligned_obj_seen_times->len; i++) {
      seen_times = g_array_index(pa->aligned_obj_seen_times, double, i);
      obj_size = g_array_index(pa->aligned_obj_size, uint64_t, i);
      adm_prob = g_array_index(pa->aligned_admission_probs, double, i);
      const double reqTProd = seen_times*the_T;
      if(reqTProd > 150) {
        // cache hit probability = 1, but numerically inaccurate to calculate
        the_C += obj_size;
      } else {
        const double expTerm = exp(reqTProd) - 1;
        const double expAdmProd = adm_prob * expTerm;
        const double tmp = expAdmProd / (1 + expAdmProd);
        the_C += obj_size * tmp;
      }
    }
    old_T = the_T;
    the_T = pa->cache_size * old_T/the_C;
  }

  // calculate object hit ratio
  double weighted_hitratio_sum = 0;
  for(size_t i = 0; i < pa->aligned_obj_seen_times->len; i++) {
    seen_times = g_array_index(pa->aligned_obj_seen_times, double, i);
    adm_prob = g_array_index(pa->aligned_admission_probs, double, i);

    const double tmp01= adaptsize_oP1(the_T,seen_times,adm_prob);
    const double tmp02= adaptsize_oP2(the_T,seen_times,adm_prob);
    double tmp;
    if(tmp01 != 0 && tmp02 == 0)
      tmp = 0.0;
    else 
      tmp= tmp01 / tmp02;
    if(tmp < 0.0)
      tmp = 0.0;
    else if (tmp > 1.0)
      tmp = 1.0;
    weighted_hitratio_sum += seen_times * tmp;
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

  g_hash_table_destroy(pa->interval_metadata);
  g_hash_table_destroy(pa->longterm_metadata);

  g_array_free(pa->aligned_admission_probs, TRUE);
  g_array_free(pa->aligned_obj_seen_times, TRUE);
  g_array_free(pa->aligned_obj_size, TRUE);
  
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

  pa->interval_metadata = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
  pa->longterm_metadata = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);

  pa->aligned_admission_probs = g_array_new(false, false, sizeof(double));
  pa->aligned_obj_seen_times = g_array_new(false, false, sizeof(double));
  pa->aligned_obj_size = g_array_new(false, false, sizeof(uint64_t));

  pa->gss_v = 1 - gss_r;
  pa->stat_size = 0;
  pa->next_reconf = pa->reconf_interval;

  admissioner->params = pa;
  admissioner->admit = adaptsize_admit;
  admissioner->free = free_adaptsize_admissioner;
  admissioner->clone = clone_adaptsize_admissioner;
  admissioner->update = adaptsize_update_stats;
  if (init_params != NULL) admissioner->init_params = strdup(init_params);

  return admissioner;
}

#ifdef __cplusplus
}
#endif