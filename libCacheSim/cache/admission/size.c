//
// Created by Juncheng on 5/29/21.
//

#include "../../include/libCacheSim/admissionAlgo.h"
#include "../../utils/include/mymath.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct size_admissioner {
  int64_t size_threshold;
} size_admission_params_t;

bool size_admit(admissioner_t *admissioner, const request_t *req) {
  size_admission_params_t *pa = (size_admission_params_t *)admissioner->params;
  if (req->obj_size < pa->size_threshold) {
    return true;
  }

  return false;
}

static void size_admissioner_parse_params(const char *init_params, size_admission_params_t *pa) {
  if (init_params == NULL) {
    pa->size_threshold = INT64_MAX;
    INFO("use default size admission: %ld\n", (long)pa->size_threshold);
  } else {
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

      if (strcasecmp(key, "size") == 0) {
        pa->size_threshold = strtoll(value, &end, 0);
        if (strlen(end) > 2) {
          ERROR("param parsing error, find string \"%s\" after number\n", end);
        }
        INFO("use size threshold: %ld\n", (long)pa->size_threshold);
      } else {
        ERROR("size admission does not have parameter %s\n", key);
      }
    }
    free(old_params_str);
  }
}

admissioner_t *clone_size_admissioner(admissioner_t *admissioner) {
  return create_size_admissioner(admissioner->init_params);
}

void free_size_admissioner(admissioner_t *admissioner) {
  size_admission_params_t *pa = admissioner->params;

  free(pa);
  if (admissioner->init_params) {
    free(admissioner->init_params);
  }
  free(admissioner);
}

admissioner_t *create_size_admissioner(const char *init_params) {
  size_admission_params_t *pa = (size_admission_params_t *)malloc(sizeof(size_admission_params_t));
  memset(pa, 0, sizeof(size_admission_params_t));
  size_admissioner_parse_params(init_params, pa);

  admissioner_t *admissioner = (admissioner_t *)malloc(sizeof(admissioner_t));
  memset(admissioner, 0, sizeof(admissioner_t));
  admissioner->params = pa;
  admissioner->admit = size_admit;
  admissioner->free = free_size_admissioner;
  admissioner->clone = clone_size_admissioner;
  if (init_params != NULL) admissioner->init_params = strdup(init_params);

  strncpy(admissioner->admissioner_name, "Size", CACHE_NAME_LEN);
  return admissioner;
}

#ifdef __cplusplus
}
#endif
