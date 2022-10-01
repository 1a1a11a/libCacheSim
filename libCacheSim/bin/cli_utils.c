

#define _GNU_SOURCE
#include <string.h>
#include <assert.h>

#include "../include/libCacheSim/reader.h"
#include "cli_utils.h"

/**
 * @brief convert the trace type string to enum
 *
 * @param args
 */
trace_type_e trace_type_str_to_enum(const char *trace_type_str) {
  if (strcasecmp(trace_type_str, "txt") == 0) {
    return PLAIN_TXT_TRACE;
  } else if (strcasecmp(trace_type_str, "csv") == 0) {
    return CSV_TRACE;
  } else if (strcasecmp(trace_type_str, "twr") == 0) {
    return TWR_TRACE;
  } else if (strcasecmp(trace_type_str, "vscsi") == 0) {
    return VSCSI_TRACE;
  } else if (strcasecmp(trace_type_str, "oracleGeneralBin") == 0 ||
             strcasecmp(trace_type_str, "oracleGeneral") == 0) {
    return ORACLE_GENERAL_TRACE;
  } else if (strcasecmp(trace_type_str, "oracleGeneralOpNS") == 0) {
    return ORACLE_GENERALOPNS_TRACE;
  } else if (strcasecmp(trace_type_str, "oracleAkamai") == 0) {
    return ORACLE_AKAMAI_TRACE;
  } else if (strcasecmp(trace_type_str, "oracleCF1") == 0) {
    return ORACLE_CF1_TRACE;
  } else if (strcasecmp(trace_type_str, "oracleSysTwrNS") == 0) {
    return ORACLE_SYS_TWRNS_TRACE;
  } else {
    ERROR("unsupported trace type: %s\n", trace_type_str);
  }
}

bool is_true(const char *arg) {
  if (strcasecmp(arg, "true") == 0 || strcasecmp(arg, "1") == 0 ||
      strcasecmp(arg, "yes") == 0 || strcasecmp(arg, "y") == 0) {
    return true;
  } else if (strcasecmp(arg, "false") == 0 || strcasecmp(arg, "0") == 0 ||
             strcasecmp(arg, "no") == 0 || strcasecmp(arg, "n") == 0) {
    return false;
  } else {
    ERROR("Invalid value: %s, expect true/false", arg);
    abort();
  }
}

void parse_reader_params(char *reader_params_str, reader_init_param_t *params) {
  params->has_header = false;
  /* whether the user has specified the has_header params */
  params->has_header_set = false;
  params->delimiter = '\0';
  params->time_field = -1;
  params->obj_id_field = -1;
  params->obj_size_field = -1;
  params->op_field = -1;
  params->ttl_field = -1;
  params->obj_id_is_num = false;

  if (reader_params_str == NULL) return;
  char *params_str = strdup(reader_params_str);
  char *old_params_str = params_str;

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ";");
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "time_col") == 0 ||
        strcasecmp(key, "time_field") == 0) {
      params->time_field = atoi(value);
    } else if (strcasecmp(key, "obj_id_col") == 0 ||
               strcasecmp(key, "obj_id_field") == 0) {
      params->obj_id_field = atoi(value);
    } else if (strcasecmp(key, "obj_size_col") == 0 ||
               strcasecmp(key, "obj_size_field") == 0 ||
               strcasecmp(key, "size_col") == 0 ||
               strcasecmp(key, "size_field") == 0) {
      params->obj_size_field = atoi(value);
    } else if (strcasecmp(key, "obj_id_is_num") == 0) {
      params->obj_id_is_num = is_true(value);
    } else if (strcasecmp(key, "header") == 0 ||
               strcasecmp(key, "has_header") == 0) {
      params->has_header = is_true(value);
      params->has_header_set = true;
    } else if (strcasecmp(key, "delimiter") == 0) {
      params->delimiter = value[0];
    } else {
      ERROR("cache does not support trace parameter %s\n", key);
      exit(1);
    }
  }

  free(old_params_str);
}

/**
 * @brief use the trace file name to verify the trace type (limited use)
 *
 * @param args
 */
void verify_trace_type(const char *trace_path, const char *trace_type_str) {
  if (strcasestr(trace_path, "oracleGeneralBin") != NULL ||
      strcasestr(trace_path, "oracleGeneral.bin") != NULL) {
    assert(strcasecmp(trace_type_str, "oracleGeneral") == 0 ||
           strcasecmp(trace_type_str, "oracleGeneralBin") == 0);
  } else if (strcasestr(trace_path, "oracleSysTwrNS") != NULL) {
    assert(strcasecmp(trace_type_str, "oracleSysTwrNS") == 0);
  } else {
    ;
  }
}
