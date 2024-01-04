//
// Created by Juncheng on 5/30/21.
//

#include "../../include/libCacheSim/admissionAlgo.h"
#include "../../utils/include/mymath.h"

typedef struct adaptsize_admissioner {
  int64_t adaptsize_threshold;
} adaptsize_admission_params_t;

bool adaptsize_admit(admissioner_t *admissioner, const request_t *req) {
  adaptsize_admission_params_t *pa =
      (adaptsize_admission_params_t *)admissioner->params;
  if (req->obj_size < pa->adaptsize_threshold) {
    return true;
  }

  return false;
}

static void adaptsize_admissioner_parse_params(
    const char *init_params, adaptsize_admission_params_t *pa) {
  if (init_params == NULL) {
    pa->adaptsize_threshold = INT64_MAX;
    INFO("use default adaptsize admission: %ld", (long) pa->adaptsize_threshold);
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

      if (strcasecmp(key, "adaptsize") == 0) {
        pa->adaptsize_threshold = strtoll(value, &end, 0);
        if (strlen(end) > 2) {
          ERROR("param parsing error, find string \"%s\" after number\n", end);
        }
      } else {
        ERROR("adaptsize admission does not have parameter %s\n", key);
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
      static_cast<adaptsize_admission_params_t *>(admissioner->params);

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
  adaptsize_admissioner_parse_params(init_params, pa);

  admissioner_t *admissioner = (admissioner_t *)malloc(sizeof(admissioner_t));
  memset(admissioner, 0, sizeof(admissioner_t));
  admissioner->params = pa;
  admissioner->admit = adaptsize_admit;
  admissioner->free = free_adaptsize_admissioner;
  admissioner->clone = clone_adaptsize_admissioner;
  if (init_params != NULL) admissioner->init_params = strdup(init_params);

  ERROR("adaptsize is not implemented\n");
  return admissioner;
}
