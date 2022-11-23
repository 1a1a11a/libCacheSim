

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/reader.h"
#include "../../utils/include/mymath.h"
#include "../../utils/include/mysys.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REPORT_INTERVAL (24 * 3600)
void simulate(reader_t *reader, cache_t *cache, int warmup_sec,
              char *ofilepath) {
  /* random seed */
  srand(time(NULL));
  set_rand_seed(rand());

  request_t *req = new_request();
  uint64_t req_cnt = 0, miss_cnt = 0;
  uint64_t last_req_cnt = 0, last_miss_cnt = 0;
  uint64_t req_byte = 0, miss_byte = 0;

  read_one_req(reader, req);
  uint64_t start_ts = (uint64_t)req->real_time;
  uint64_t last_report_ts = warmup_sec;

  double start_time = -1;
  while (req->valid) {
    req->real_time -= start_ts;
    if (req->real_time <= warmup_sec) {
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
    if (cache->get(cache, req) != cache_ck_hit) {
      miss_cnt++;
      miss_byte += req->obj_size;
    }
    if (req->real_time - last_report_ts >= REPORT_INTERVAL &&
        req->real_time != 0) {
      INFO(
          "%.2lf hour: %lu requests, miss ratio %.4lf, interval miss ratio "
          "%.4lf\n",
          (double)req->real_time / 3600, (unsigned long)req_cnt,
          (double)miss_cnt / req_cnt,
          (double)(miss_cnt - last_miss_cnt) / (req_cnt - last_req_cnt));
      last_miss_cnt = miss_cnt;
      last_req_cnt = req_cnt;
      last_report_ts = (int64_t)req->real_time;
    }

    read_one_req(reader, req);
  }

  double runtime = gettime() - start_time;

  char output_str[1024];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
  snprintf(output_str, 1024,
           "%.2lf hour: %12s, cache size %d, %s, %lu "
           "requests, miss ratio %.4lf, "
           "throughput %.2lf MQPS\n",
           (double)req->real_time / 3600.0, reader->trace_path,
           (int)(cache->cache_size), cache->cache_name,
           (unsigned long)req_cnt, (double)miss_cnt / req_cnt,
           (double)req_cnt / 1000000.0 / runtime);
#pragma GCC diagnostic pop
  INFO("%s", output_str);

  FILE *output_file = fopen(ofilepath, "a");
  fprintf(output_file, "%s\n", output_str);
  fclose(output_file);

#if defined(TRACK_EVICTION_R_AGE) || defined(TRACK_EVICTION_V_AGE)
  print_eviction_age(cache);
#endif
}

#ifdef __cplusplus
}
#endif

