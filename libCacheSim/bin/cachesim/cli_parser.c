
#define _GNU_SOURCE
#include <string.h>

#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/reader.h"
#include "internal.h"

#ifdef __cplusplus
extern "C" {
#endif

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
#define OUTPUT_STR_LEN 1024
  char output_str[OUTPUT_STR_LEN];
  int n = snprintf(
      output_str, OUTPUT_STR_LEN - 1,
      "trace path: %s, trace_type %s, eviction %s, ofilepath "
      "%s, %d threads, warmup %d sec",
      args->trace_path, trace_type_str[args->trace_type], // cache_size_str,
      args->eviction_algo, args->ofilepath, args->n_thread, args->warmup_sec);

  if (args->trace_type_params != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", trace-type-params: %s", args->trace_type_params);

  if (args->admission_algo != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", admission: %s",
                  args->admission_algo);

  if (args->admission_params != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", admission-params: %s", args->admission_params);

  if (args->eviction_params != NULL)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", eviction-params: %s", args->eviction_params);

  if (args->use_ttl)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", use ttl");

  if (args->ignore_obj_size)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", ignore object size");

  if (args->consider_obj_metadata)
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1,
                  ", consider object metadata");

  snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, "\n");

  INFO("%s", output_str);

#undef OUTPUT_STR_LEN
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
    reset_reader(reader);
    long wss = cal_working_set_size(reader, args->ignore_obj_size);
    double s[N_AUTO_CACHE_SIZE] = {0.0001, 0.0003, 0.001, 0.003,
                                   0.01,   0.03,   0.1,   0.3};
    for (int i = 0; i < N_AUTO_CACHE_SIZE; i++) {
      args->cache_sizes[i] = (long)(wss * s[i]);
    }
    args->n_cache_size = N_AUTO_CACHE_SIZE;

    reset_reader(reader);
  }
}

#ifdef __cplusplus
}
#endif
