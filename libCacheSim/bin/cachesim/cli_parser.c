
#define _GNU_SOURCE
#include <string.h>

#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/reader.h"
#include "internal.h"

#ifdef __cplusplus
extern "C" {
#endif

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
 * @brief convert the trace type string to enum
 *
 * @param args
 */
void trace_type_str_to_enum(struct arguments *args) {
  if (strcasecmp(args->trace_type_str, "txt") == 0) {
    args->trace_type = PLAIN_TXT_TRACE;
  } else if (strcasecmp(args->trace_type_str, "csv") == 0) {
    args->trace_type = CSV_TRACE;
  } else if (strcasecmp(args->trace_type_str, "twr") == 0) {
    args->trace_type = TWR_TRACE;
  } else if (strcasecmp(args->trace_type_str, "vscsi") == 0) {
    args->trace_type = VSCSI_TRACE;
  } else if (strcasecmp(args->trace_type_str, "oracleGeneralBin") == 0 ||
             strcasecmp(args->trace_type_str, "oracleGeneral") == 0) {
    args->trace_type = ORACLE_GENERAL_TRACE;
  } else if (strcasecmp(args->trace_type_str, "oracleGeneralOpNS") == 0) {
    args->trace_type = ORACLE_GENERALOPNS_TRACE;
  } else if (strcasecmp(args->trace_type_str, "oracleAkamai") == 0) {
    args->trace_type = ORACLE_AKAMAI_TRACE;
  } else if (strcasecmp(args->trace_type_str, "oracleCF1") == 0) {
    args->trace_type = ORACLE_CF1_TRACE;
  } else if (strcasecmp(args->trace_type_str, "oracleSysTwrNS") == 0) {
    args->trace_type = ORACLE_SYS_TWRNS_TRACE;
  } else {
    printf("unsupported trace type: %s\n", args->trace_type_str);
    exit(1);
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

  free(params_str);
}
/**
 * @brief convert cache size string to byte, e.g., 100MB -> 100 * 1024 * 1024
 * the cache size can be an integer or a string with suffix KB/MB/GB
 *
 * @param cache_size_str
 * @return unsigned long
 */
static unsigned long conv_size_str_to_byte_ul(char *cache_size_str) {
  if (strcasecmp(cache_size_str, "auto") == 0) {
    return 0;
  }

  if (strcasestr(cache_size_str, "kb") != NULL ||
      cache_size_str[strlen(cache_size_str) - 1] == 'k' ||
      cache_size_str[strlen(cache_size_str) - 1] == 'K') {
    return strtoul(cache_size_str, NULL, 10) * KiB;
  } else if (strcasestr(cache_size_str, "mb") != NULL ||
             cache_size_str[strlen(cache_size_str) - 1] == 'm' ||
             cache_size_str[strlen(cache_size_str) - 1] == 'M') {
    return strtoul(cache_size_str, NULL, 10) * MiB;
  } else if (strcasestr(cache_size_str, "gb") != NULL ||
             cache_size_str[strlen(cache_size_str) - 1] == 'g' ||
             cache_size_str[strlen(cache_size_str) - 1] == 'G') {
    return strtoul(cache_size_str, NULL, 10) * GiB;
  } else if (strcasestr(cache_size_str, "tb") != NULL ||
             cache_size_str[strlen(cache_size_str) - 1] == 't' ||
             cache_size_str[strlen(cache_size_str) - 1] == 'T') {
    return strtoul(cache_size_str, NULL, 10) * TiB;
  }

  long cache_size = strtol(cache_size_str, NULL, 10);
  cache_size = cache_size == -1 ? 0 : cache_size;

  return (unsigned long)cache_size;
}

/** @brief convert the cache size string to byte,
 * example input1: 1024,2048,4096,8192
 * example input2: 1GB,2GB,4GB,8GB
 * example input3: 1024
 * example input4: 0
 *
 * @param cache_size_str
 * @param args
 * @return int
 */
int conv_cache_sizes(char *cache_size_str, struct arguments *args) {
  char *token = strtok(cache_size_str, ",");
  args->n_cache_size = 0;
  while (token != NULL) {
    args->cache_sizes[args->n_cache_size++] = conv_size_str_to_byte_ul(token);
    token = strtok(NULL, ",");
  }

  if (args->n_cache_size == 1 && args->cache_sizes[0] == 0) {
    args->n_cache_size = 0;
  }

  return args->n_cache_size;
}

/**
 * @brief print the parsed arguments
 *
 * @param args
 */
void print_parsed_args(struct arguments *args) {
  char cache_size_str[1024];
  int n = 0;
  for (int i = 0; i < args->n_cache_size; i++) {
    n += snprintf(cache_size_str + n, 1023 - n, "%lu,", args->cache_sizes[i]);
    assert(n < 1024);
  }

#define OUTPUT_STR_LEN (1024 * 4)
  char output_str[OUTPUT_STR_LEN];
  n = snprintf(
      output_str, OUTPUT_STR_LEN - 1,
      "trace path: %s, trace_type %s, cache size %s eviction %s, ofilepath "
      "%s, %d threads, warmup %lu sec",
      args->trace_path, trace_type_str[args->trace_type], cache_size_str,
      args->eviction_algo, args->ofilepath, args->n_thread, args->warmup_sec);

  if (args->trace_type_params != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", trace_type_params: %s",
                  args->trace_type_params);

  if (args->admission_algo != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", admission: %s",
                  args->admission_algo);

  if (args->admission_params != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", admission_params: %s",
                  args->admission_params);

  if (args->eviction_params != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", eviction_params: %s",
                  args->eviction_params);

  if (args->use_ttl)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", use ttl");

  if (args->ignore_obj_size)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", ignore object size");

  if (args->consider_obj_metadata)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", consider object metadata");

  snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, "\n");

  INFO("%s", output_str);
}

/**
 * @brief use the trace file name to verify the trace type (limited use)
 *
 * @param args
 */
void verify_trace_type(struct arguments *args) {
  if (strcasestr(args->trace_path, "oracleGeneralBin") != NULL ||
      strcasestr(args->trace_path, "oracleGeneral.bin") != NULL) {
    assert(strcasecmp(args->trace_type_str, "oracleGeneral") == 0 ||
           strcasecmp(args->trace_type_str, "oracleGeneralBin") == 0);
  } else if (strcasestr(args->trace_path, "oracleSysTwrNS") != NULL) {
    assert(strcasecmp(args->trace_type_str, "oracleSysTwrNS") == 0);
  } else {
    ;
  }
}

static long cal_working_set_size(reader_t *reader, bool ignore_obj_size) {
  long wss = 0;
  request_t *req = new_request();
  GHashTable *obj_table = g_hash_table_new(g_direct_hash, g_direct_equal);

  INFO("calculating working set size...\n");
  while (read_one_req(reader, req) == 0) {
    if (g_hash_table_contains(obj_table, (gconstpointer)req->obj_id)) {
      continue;
    }

    g_hash_table_add(obj_table, (gpointer)req->obj_id);

    if (ignore_obj_size) {
      wss += 1;
    } else {
      wss += req->obj_size;
    }
  }
  INFO("working set size: %ld %s\n", wss,
       ignore_obj_size ? "objects" : "bytes");

  return wss;
}

void set_cache_size(struct arguments *args, reader_t *reader) {
#define N_AUTO_CACHE_SIZE 8
  if (args->n_cache_size == 0) {
    if (set_hard_code_cache_size(args)) {
      /* find the hard-coded cache size */
      return;
    }

    // detect cache size from the trace
    long wss = cal_working_set_size(reader, args->ignore_obj_size);
    double s[N_AUTO_CACHE_SIZE] = {0.0001, 0.0003, 0.001, 0.003,
                                   0.01,   0.03,   0.1,   0.3};
    for (int i = 0; i < N_AUTO_CACHE_SIZE; i++) {
      args->cache_sizes[i] = (long)(wss * s[i]);
    }
    args->n_cache_size = N_AUTO_CACHE_SIZE;
  }
}

#ifdef __cplusplus
}
#endif
