
#define _GNU_SOURCE
#include <string.h>

#include "../../include/libCacheSim/enum.h"
#include "../../include/libCacheSim/reader.h"
#include "internal.h"

#ifdef __cplusplus
extern "C" {
#endif

static void cal_working_set_size(reader_t *reader, int64_t *wss_obj,
                                 int64_t *wss_byte);
static void set_cache_size(struct arguments *args, reader_t *reader);

/**
 * @brief parse the command line eviction_algo arguments
 * the given input is a string, e.g., "LRU,LFU"
 * this function parses the string and stores the parsed eviction algorithms
 * into the args->eviction_algo array
 * and the number of eviction algorithms is stored in args->n_eviction_algo
 */
void parse_eviction_algo(struct arguments *args, const char *arg) {
#define MAX_ALGO_LEN 4096
  char data[MAX_ALGO_LEN] = {0};
  memcpy(data, arg, strlen(arg));
  char *str = data;
  char *algo = NULL;

  int n_algo = 0;
  while (str != NULL && str[0] != '\0') {
    /* different algorithms are separated by comma */
    algo = strsep(&str, ",");
    args->eviction_algo[n_algo++] = strdup(algo);
  }
  args->n_eviction_algo = n_algo;

#undef MAX_ALGO_LEN
}

/**
 *
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
  long wss = 0;
  args->n_cache_size = 0;
  while (token != NULL) {
    if (strchr(token, '.') != NULL) {
      // input is a float
      if (wss == 0) {
        int64_t wss_obj = 0, wss_byte = 0;
        cal_working_set_size(args->reader, &wss_obj, &wss_byte);
        wss = args->ignore_obj_size ? wss_obj : wss_byte;
      }
      args->cache_sizes[args->n_cache_size++] = (uint64_t)(wss * atof(token));
    } else {
      args->cache_sizes[args->n_cache_size++] = conv_size_str_to_byte_ul(token);
    }

    token = strtok(NULL, ",");
  }

  if (args->n_cache_size == 1 && args->cache_sizes[0] == 0) {
    set_cache_size(args, args->reader);
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
      "trace path: %s, trace_type %s, ofilepath "
      "%s, %d threads, warmup %d sec, total %d algo x %d size = %d caches",
      args->trace_path, trace_type_str[args->trace_type], args->ofilepath,
      args->n_thread, args->warmup_sec, args->n_eviction_algo,
      args->n_cache_size, args->n_eviction_algo * args->n_cache_size);

  for (int i = 0; i < args->n_eviction_algo; i++) {
    n += snprintf(output_str + n, OUTPUT_STR_LEN - n - 1, ", %s",
                  args->eviction_algo[i]);
  }

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

static void cal_working_set_size(reader_t *reader, int64_t *wss_obj,
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

static void set_cache_size(struct arguments *args, reader_t *reader) {
#define N_AUTO_CACHE_SIZE 8
  // if (set_hard_code_cache_size(args)) {
  //   /* find the hard-coded cache size */
  //   return;
  // }

  // detect cache size from the trace
  int n_cache_sizes = 0;
  int64_t wss_obj = 0, wss_byte = 0;
  cal_working_set_size(reader, &wss_obj, &wss_byte);
  int64_t wss = args->ignore_obj_size ? wss_obj : wss_byte;
  double s[N_AUTO_CACHE_SIZE] = {0.001, 0.003, 0.01, 0.03, 0.1, 0.2, 0.4, 0.8};
  for (int i = 0; i < N_AUTO_CACHE_SIZE; i++) {
    if (args->ignore_obj_size) {
      if ((long)(wss_obj * s[i]) > 4) {
        args->cache_sizes[n_cache_sizes++] = (long)(wss * s[i]);
      }
    } else {
      if ((long)(wss_obj * s[i]) >= 100) {
        args->cache_sizes[n_cache_sizes++] = (long)(wss * s[i]);
      }
    }
  }
  args->n_cache_size = n_cache_sizes;

  if (args->n_cache_size == 0) {
    printf("working set %ld too small\n", wss);
    exit(0);
  }
}

#ifdef __cplusplus
}
#endif
