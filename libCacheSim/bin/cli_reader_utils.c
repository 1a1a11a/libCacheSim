

#define _GNU_SOURCE
#include "cli_reader_utils.h"

#include <assert.h>
#include <string.h>

#include "../include/libCacheSim/reader.h"
#include "../utils/include/mystr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief convert the trace type string to enum
 *
 * @param args
 */
trace_type_e trace_type_str_to_enum(const char *trace_type_str,
                                    const char *trace_path) {
  if (strcasecmp(trace_type_str, "auto") == 0) {
    trace_type_e trace_type = detect_trace_type(trace_path);
    if (trace_type == UNKNOWN_TRACE) {
      ERROR(
          "cannot detect trace type from trace path %s, "
          "please specify the trace type manually\n",
          trace_path);
    }
  } else if (strcasecmp(trace_type_str, "txt") == 0) {
    return PLAIN_TXT_TRACE;
  } else if (strcasecmp(trace_type_str, "csv") == 0) {
    return CSV_TRACE;
  } else if (strcasecmp(trace_type_str, "binary") == 0) {
    return BIN_TRACE;
  } else if (strcasecmp(trace_type_str, "lcs") == 0) {
    // libCacheSim trace
    return LCS_TRACE;
  } else if (strcasecmp(trace_type_str, "twr") == 0) {
    return TWR_TRACE;
  } else if (strcasecmp(trace_type_str, "twrNS") == 0) {
    return TWRNS_TRACE;
  } else if (strcasecmp(trace_type_str, "vscsi") == 0) {
    return VSCSI_TRACE;
  } else if (strcasecmp(trace_type_str, "CF1") == 0) {
    return CF1_TRACE;
  } else if (strcasecmp(trace_type_str, "oracleWiki16u") == 0) {
    return ORACLE_WIKI16u_TRACE;
  } else if (strcasecmp(trace_type_str, "oracleWiki19u") == 0) {
    return ORACLE_WIKI19u_TRACE;
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
  } else if (strcasecmp(trace_type_str, "valpinTrace") == 0){
    return VALPIN_TRACE;
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

/**
 * @brief parse the reader parameters, mostly this is used by csv traces
 *
 * @param reader_params_str
 * @param params
 */
void parse_reader_params(const char *reader_params_str,
                         reader_init_param_t *params) {
  params->delimiter = '\0';
  params->obj_id_is_num = false;

  if (reader_params_str == NULL) return;
  char *params_str = strdup(reader_params_str);
  char *old_params_str = params_str;
  char *end;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by '=' */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // skip the white space and comma
    while (params_str != NULL && (*params_str == ' ' || *params_str == ',')) {
      params_str++;
    }

    key = replace_char(key, '_', '-');

    if (strcasecmp(key, "time-col") == 0 ||
        strcasecmp(key, "time-field") == 0) {
      params->time_field = (int)strtol(value, &end, 0);
      if (strlen(end) > 2)
        ERROR("param parsing error, find string \"%s\" after number\n", end);
    } else if (strcasecmp(key, "obj-id-col") == 0 ||
               strcasecmp(key, "obj-id-field") == 0) {
      params->obj_id_field = (int)strtol(value, &end, 0);
      if (strlen(end) > 2)
        ERROR("param parsing error, find string \"%s\" after number\n", end);
    } else if (strcasecmp(key, "obj-size-col") == 0 ||
               strcasecmp(key, "obj-size-field") == 0 ||
               strcasecmp(key, "size-col") == 0 ||
               strcasecmp(key, "size-field") == 0) {
      params->obj_size_field = (int)strtol(value, &end, 0);
      if (strlen(end) > 2)
        ERROR("param parsing error, find string \"%s\" after number\n", end);
    } else if (strcasecmp(key, "cnt-col") == 0 ||
               strcasecmp(key, "cnt-field") == 0) {
      params->cnt_field = (int)strtol(value, &end, 0);
      if (strlen(end) > 2)
        ERROR("param parsing error, find string \"%s\" after number\n", end);
    } else if (strcasecmp(key, "next-access-col") == 0 ||
               strcasecmp(key, "next-access-field") == 0) {
      params->next_access_vtime_field = (int)strtol(value, &end, 0);
      if (strlen(end) > 2)
        ERROR("param parsing error, find string \"%s\" after number\n", end);
    } else if (strcasecmp(key, "obj-id-is-num") == 0) {
      params->obj_id_is_num = is_true(value);
    } else if (strcasecmp(key, "header") == 0 ||
               strcasecmp(key, "has-header") == 0) {
      params->has_header = is_true(value);
      params->has_header_set = true;
    } else if (strcasecmp(key, "format") == 0) {
      params->binary_fmt_str = strdup(value);
    } else if (strcasecmp(key, "delimiter") == 0) {
      /* user input: k1=v1, delimiter=;, k2=v2 */
      params->delimiter = value[0];
      if (value[0] == '\0') {
        /* user input: k1=v1, delimiter=,, k2=v2*/
        params->delimiter = ',';
      } else if (value[0] == '\\') {
        if (strlen(value) == 1) {
          /* user input: k1=v1, delimiter=\,, k2=v2*/
          params->delimiter = ',';
        } else if (strlen(value) == 2) {
          if (value[1] == 't') {
            /* user input: k1=v1, delimiter=\t, k2=v2*/
            params->delimiter = '\t';
          } else if (value[1] == ',') {
            ERROR("can I reach here?\n");
            params->delimiter = ',';
          } else {
            ERROR("unsupported delimiter: '%s'\n", value);
          }
        } else {
          ERROR("unsupported delimiter: '%s'\n", value);
        }
      }
    } else {
      ERROR("cache does not support trace parameter %s\n", key);
      exit(1);
    }
  }

  free(old_params_str);
}

/**
 * @brief detect the trace type from the trace path
 *
 * @param trace_path
 * @return trace_type_e
 */
trace_type_e detect_trace_type(const char *trace_path) {
  trace_type_e trace_type = UNKNOWN_TRACE;

  if (strcasestr(trace_path, "oracleGeneralBin") != NULL ||
      strcasestr(trace_path, "oracleGeneral.bin") != NULL ||
      strcasestr(trace_path, "bin.oracleGeneral") != NULL ||
      strcasestr(trace_path, "oracleGeneral.zst") != NULL ||
      strcasestr(trace_path, "oracleGeneral.") != NULL ||
      strcasecmp(trace_path + strlen(trace_path) - 13, "oracleGeneral") == 0) {
    trace_type = ORACLE_GENERAL_TRACE;
  } else if (strcasestr(trace_path, ".vscsi") != NULL) {
    trace_type = VSCSI_TRACE;
  } else if (strcasestr(trace_path, "oracleGeneralOpNS") != NULL) {
    trace_type = ORACLE_GENERALOPNS_TRACE;
  } else if (strcasestr(trace_path, "oracleCF1") != NULL) {
    trace_type = ORACLE_CF1_TRACE;
  } else if (strcasestr(trace_path, "oracleAkamai") != NULL) {
    trace_type = ORACLE_AKAMAI_TRACE;
  } else if (strcasestr(trace_path, "oracleWiki16u") != NULL) {
    trace_type = ORACLE_WIKI16u_TRACE;
  } else if (strcasestr(trace_path, "oracleWiki19u") != NULL) {
    trace_type = ORACLE_WIKI19u_TRACE;
  } else if (strcasestr(trace_path, ".twr.") != NULL) {
    trace_type = TWR_TRACE;
  } else if (strcasestr(trace_path, ".twrNS.") != NULL) {
    trace_type = TWRNS_TRACE;
  } else if (strcasestr(trace_path, "oracleSysTwrNS") != NULL) {
    trace_type = ORACLE_SYS_TWRNS_TRACE;
  } else {
    trace_type = UNKNOWN_TRACE;
  }

  INFO("detecting trace type: %s\n", trace_type_str[trace_type]);
  return trace_type;
}

/**
 * @brief detect whether we should disable object metadata
 *
 */
#define N_TEST 1024
bool should_disable_obj_metadata(reader_t *reader) {
  bool disable_obj_metadata = true;
  request_t *req = new_request();
  for (int i = 0; i < N_TEST; i++) {
    if (read_one_req(reader, req) != 0) break;

    if (req->obj_size > 1) {
      disable_obj_metadata = false;
      break;
    }
  }
  free_request(req);
  reset_reader(reader);

  return disable_obj_metadata;
}
#undef N_TEST

void cal_working_set_size(reader_t *reader, int64_t *wss_obj,
                          int64_t *wss_byte) {
  reset_reader(reader);
  request_t *req = new_request();
  GHashTable *obj_table = g_hash_table_new(g_direct_hash, g_direct_equal);
  *wss_obj = 0;
  *wss_byte = 0;

  // sample the object space in case there are too many objects
  // which can cause a crash
  int scaling_factor = 1;
  if (reader->file_size > 5 * GiB) {
    scaling_factor = 101;
  } else if (reader->file_size > 1 * GiB) {
    scaling_factor = 11;
  }

  INFO("calculating working set size...\n");
  while (read_one_req(reader, req) == 0) {
    if (scaling_factor > 1 && req->obj_id % scaling_factor != 0) {
      continue;
    }

    if (g_hash_table_contains(obj_table, (gconstpointer)req->obj_id)) {
      continue;
    }

    g_hash_table_add(obj_table, (gpointer)req->obj_id);

    *wss_obj += 1;
    *wss_byte += req->obj_size;
  }
  *wss_obj *= scaling_factor;
  *wss_byte *= scaling_factor;
  INFO("working set size: %ld object %ld byte\n", *wss_obj, *wss_byte);

  free_request(req);
  reset_reader(reader);
}

/**
 * @brief Create a reader from the parameters
 *
 * @param trace_type_str
 * @param trace_path
 * @param trace_type_params
 * @param n_req
 * @param ignore_obj_size
 * @param sample_ratio
 * @return reader_t*
 */
reader_t *create_reader(const char *trace_type_str, const char *trace_path,
                        const char *trace_type_params, const int64_t n_req,
                        const bool ignore_obj_size, const int sample_ratio) {
  /* convert trace type string to enum */
  trace_type_e trace_type = trace_type_str_to_enum(trace_type_str, trace_path);

  reader_init_param_t reader_init_params;
  memset(&reader_init_params, 0, sizeof(reader_init_params));
  reader_init_params.ignore_obj_size = ignore_obj_size;
  reader_init_params.ignore_size_zero_req = true;
  reader_init_params.obj_id_is_num = true;
  reader_init_params.cap_at_n_req = n_req;
  reader_init_params.sampler = NULL;

  parse_reader_params(trace_type_params, &reader_init_params);

  if (sample_ratio > 0 && sample_ratio < 1 - 1e-6) {
    sampler_t *sampler = create_spatial_sampler(sample_ratio);
    reader_init_params.sampler = sampler;
  }

  reader_t *reader = setup_reader(trace_path, trace_type, &reader_init_params);

  return reader;
}

#ifdef __cplusplus
}
#endif
