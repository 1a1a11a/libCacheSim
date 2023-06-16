#pragma once

#include "utils.h"

using namespace std;

namespace analysis {


static struct analysis_param parse_param(string param_string) {
  struct analysis_param params = default_param();
  char *params_str = strdup(param_string.c_str());
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
    if (strcasecmp(key, "time_window") == 0) {
      params.time_window = strtol(value, &end, 10);
      CHECK_PARSER_STATE(end);
    } else if (strcasecmp(key, "warmup_time") == 0) {
      params.warmup_time = strtol(value, &end, 10);
      CHECK_PARSER_STATE(end);
    } else if (strcasecmp(key, "sample_ratio") == 0) {
      /* used for accessPattern only */
      params.sample_ratio = strtol(value, &end, 10);
      CHECK_PARSER_STATE(end);
    } else if (strcasecmp(key, "n_hit") == 0) {
      CHECK_PARSER_STATE(end);
    } else {
      ERROR("analysis does not have parameter %s\n", key);
      exit(1);
    }
  }

  free(old_params_str);

  return params;
}

};  // namespace analysis