#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/reader.h"
#include "../../utils/include/mymath.h"
#include "../../utils/include/mystr.h"
#include "../../utils/include/mysys.h"

#ifdef __cplusplus
extern "C" {
#endif

void print_head_requests(request_t *req, uint64_t req_cnt) {
  if (req_cnt < 2) {
    print_request(req);
  }
}

void simulate(reader_t *reader, cache_t *cache, int report_interval, int warmup_sec, char *ofilepath,
              bool ignore_obj_size, bool print_head_req) {
  /* random seed */
  srand(time(NULL));
  set_rand_seed(rand());

  request_t *req = new_request();
  uint64_t req_cnt = 0, miss_cnt = 0;
  uint64_t last_req_cnt = 0, last_miss_cnt = 0;
  uint64_t req_byte = 0, miss_byte = 0;

  read_one_req(reader, req);
  uint64_t start_ts = (uint64_t)req->clock_time;
  uint64_t last_report_ts = warmup_sec;

  char cache_stat_name[CACHE_STAT_NAME_ARRAY_LEN];
  generate_cache_name(cache, cache_stat_name);

  double start_time = -1;
  while (req->valid) {
    if (print_head_req) {
      print_head_requests(req, req_cnt);
    }

    req->clock_time -= start_ts;
    if (req->clock_time <= warmup_sec) {
      cache->get(cache, req);
      read_one_req(reader, req);
      continue;
    } else {
      if (start_time < 0) {
        start_time = gettime();
      }
    }

    req_cnt++;
    req_byte += req->obj_size;
    if (cache->get(cache, req) == false) {
      miss_cnt++;
      miss_byte += req->obj_size;
    }
    if (req->clock_time - last_report_ts >= report_interval &&
        req->clock_time != 0) {
      INFO(
          "%s %s %.2lf hour: %lu requests, miss ratio %.4lf, interval miss "
          "ratio "
          "%.4lf\n",
          mybasename(reader->trace_path), cache_stat_name,
          (double)req->clock_time / 3600, (unsigned long)req_cnt,
          (double)miss_cnt / req_cnt,
          (double)(miss_cnt - last_miss_cnt) / (req_cnt - last_req_cnt));
      last_miss_cnt = miss_cnt;
      last_req_cnt = req_cnt;
      last_report_ts = (int64_t)req->clock_time;
    }

    read_one_req(reader, req);
  }

  double runtime = gettime() - start_time;

  char output_str[1024];
  char size_str[8];

  

  if (!ignore_obj_size)
    convert_size_to_str(cache->cache_size, size_str);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
  if (!ignore_obj_size) {
    snprintf(output_str, 1024,
             "%s %s cache size %8s, %16lu req, miss ratio %.4lf, throughput "
             "%.2lf MQPS\n",
            reader->trace_path, cache_stat_name, size_str,
            (unsigned long)req_cnt, (double)miss_cnt / (double)req_cnt,
            (double)req_cnt / 1000000.0 / runtime);
  } else {
    snprintf(output_str, 1024,
             "%s %s cache size %8ld, %16lu req, miss ratio %.4lf, throughput "
             "%.2lf MQPS\n",
            reader->trace_path, cache_stat_name, cache->cache_size,
            (unsigned long)req_cnt, (double)miss_cnt / (double)req_cnt,
            (double)req_cnt / 1000000.0 / runtime);
  }

#pragma GCC diagnostic pop
  printf("%s", output_str);
  char *output_dir = rindex(ofilepath, '/');
  if (output_dir != NULL) {
    size_t dir_length = output_dir - ofilepath;
    char dir_path[1024];
    snprintf(dir_path, dir_length + 1, "%s", ofilepath);
    create_dir(dir_path);
  }
  FILE *output_file = fopen(ofilepath, "a");
  if (output_file == NULL) {
    ERROR("cannot open file %s %s\n", ofilepath, strerror(errno));
    exit(1);
  }
  fprintf(output_file, "%s", output_str);
  fclose(output_file);

#if defined(TRACK_EVICTION_V_AGE)
  while (cache->get_occupied_byte(cache) > 0) {
    cache->evict(cache, req);
  }

#endif
  free_request(req);
  cache->cache_free(cache);
}

#ifdef __cplusplus
}
#endif
