#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <cstdint>
#include "../../include/libCacheSim/admissionAlgo.h"
#include "adaptsize/adaptsize.h"

typedef struct adaptsize_admissioner {
  uint64_t max_iteration;
  uint64_t reconf_interval;
  Adaptsize adaptsize;
} adaptsize_admission_params_t;

static const char* DEFAULT_PARAMS="max-iteration=15,reconf-interval=30000";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
void free_adaptsize_admissioner(admissioner_t *admissioner);
admissioner_t *clone_adaptsize_admissioner(admissioner_t *admissioner);
admissioner_t *create_adaptsize_admissioner(const char *init_params);
void adaptsize_update_stats(admissioner_t *admissioner, const request_t *req, const uint64_t cache_size);
bool adaptsize_admit(admissioner_t *admissioner, const request_t *req);
// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

/** 
 * @brief This function get called for every lookup to update adaptsize stats
 * @param admissioner  
 * @param req 
 * @param cache_size current cache size
 */
void adaptsize_update_stats(admissioner_t *admissioner, const request_t *req, const uint64_t cache_size) {
  adaptsize_admission_params_t *pa = (adaptsize_admission_params_t *)admissioner->params;
  pa->adaptsize.updateStats(req, cache_size);
}

/** 
 * @brief This function used by cache can_admit(), get called everytime an object is to be admitted. 
 * @param pa Adaptsize current data  
 * @return true cache would be admitted 
 */
bool adaptsize_admit(admissioner_t *admissioner, const request_t *req) {
  adaptsize_admission_params_t *pa =
      (adaptsize_admission_params_t *)admissioner->params;
  return pa->adaptsize.admit(req);
}

/**
 * @brief Parsing params for adaptsize. 
 * @param init_params Adaptsize spesific parameter
 * @param pa Adaptsize current data  
 */
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
      } else if (strcasecmp(key, "reconf-interval") == 0) {
        pa->reconf_interval = strtoull(value, &end, 10);
      } else if (strcasecmp(key, "print") == 0){
        printf("max-iteration=%lu,reconf-interval=%lu", pa->max_iteration, pa->reconf_interval);
        exit(0);
      } else {
        ERROR("adaptsize admission does not have parameter %s\n", key);
        exit(1);
      }
    }
    free(old_params_str);
  }
}

/**
 * @brief clone adaptsize params with its parameter 
 * @param admissioner
 * @return admissioner with the same parameter  
 */
admissioner_t *clone_adaptsize_admissioner(admissioner_t *admissioner) {
  return create_adaptsize_admissioner((const char *)admissioner->init_params);
}

/**
 * @brief free adaptsize admissioner 
 * @param admissioner 
 */
void free_adaptsize_admissioner(admissioner_t *admissioner) {
  adaptsize_admission_params_t *pa = 
    (adaptsize_admission_params_t*)(admissioner->params);
  
  free(pa);

  if (admissioner->init_params) {
    free(admissioner->init_params);
  }
  free(admissioner);
}

/**
 * @brief Init adaptsize admissioner 
 * @param init_params Adaptsize spesific parameter
 * @return 
 */
admissioner_t *create_adaptsize_admissioner(const char *init_params) {
  adaptsize_admission_params_t *pa = (adaptsize_admission_params_t *)malloc(
      sizeof(adaptsize_admission_params_t));
  memset(pa, 0, sizeof(adaptsize_admission_params_t));

  adaptsize_admissioner_parse_params(DEFAULT_PARAMS,  pa);
  if (init_params != NULL) {
    adaptsize_admissioner_parse_params(init_params, pa);
  }

  admissioner_t *admissioner = (admissioner_t *)malloc(sizeof(admissioner_t));
  memset(admissioner, 0, sizeof(admissioner_t));

  pa->adaptsize = Adaptsize(pa->max_iteration, pa->reconf_interval);

  admissioner->params = pa;
  admissioner->admit = adaptsize_admit;
  admissioner->free = free_adaptsize_admissioner;
  admissioner->clone = clone_adaptsize_admissioner;
  admissioner->update = adaptsize_update_stats;
  if (init_params != NULL) admissioner->init_params = strdup(init_params);

  strncpy(admissioner->admissioner_name, "AdaptSize", CACHE_NAME_LEN);
  return admissioner;
}