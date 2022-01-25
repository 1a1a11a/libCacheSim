

#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <assert.h>

#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/simulator.h"

#include "cachesim.h"
#include "params.h"
#include "utils.h"


void run_cache(reader_t *reader, cache_t *cache) {
  request_t *req = new_request();
  uint64_t req_cnt = 0, miss_cnt = 0;
  uint64_t req_byte = 0, miss_byte = 0;

  read_one_req(reader, req);
  int64_t start_ts = (int64_t) req->real_time, last_report_ts = 0;

  /* skip half of the requests */
  long n_skipped = 0;
  // for (int i = 0; i < reader->n_total_req / 2; i++) {
//  while (req->real_time - start_ts < 86400 * 4) {
//    n_skipped += 1;
//    req->real_time -= start_ts;
//    cache->get(cache, req);
//    read_one_req(reader, req);
  // }

  double start_time = gettime();
  while (req->valid) {
    req->real_time -= start_ts;

    req_cnt++;
    req_byte += req->obj_size;
    if (cache->get(cache, req) != cache_ck_hit) {
      miss_cnt++;
      miss_byte += req->obj_size;
//      printf("%ld %ld miss - cache size %d/%d\n", req_cnt, req->obj_id, cache->occupied_size, cache->cache_size);
    }
    else {
//      printf("%ld %ld hi - cache size %d/%d\n", req_cnt-1, req->obj_id, cache->occupied_size, cache->cache_size);
//      break;
    }
//    if (req_cnt > 20000)
//      break;

    if (req->real_time - last_report_ts >= 3600 * 6 && req->real_time != 0) {
      INFO("ts %lu: %lu requests, miss cnt %lu %.4lf, byte miss ratio %.4lf\n",
           (unsigned long) req->real_time, (unsigned long) req_cnt, (unsigned long) miss_cnt,
           (double) miss_cnt / req_cnt, (double) miss_byte / req_byte);

#ifdef TRACK_EVICTION_AGE
      print_eviction_age(cache);
#endif
      last_report_ts = (int64_t) req->real_time;
    }

    read_one_req(reader, req);
  }

  double runtime = gettime() - start_time;
  printf("runtime %lf s\n", runtime);
  INFO("ts %lu: %lu requests, miss cnt %lu %.4lf, miss byte %lu %.4lf,"
       " throughput (MQPS): %.2lf, skipped %ld requests\n",
       (unsigned long) req->real_time, (unsigned long) req_cnt,
       (unsigned long) miss_cnt, (double) miss_cnt / req_cnt,
       (unsigned long) miss_byte, (double) miss_byte/req_byte,
       (double) req_cnt / 1000000.0 / runtime,
       n_skipped);
}

int main(int argc, char **argv) {

  //  reader_init_param_t init_params = {.obj_id_field=6, .has_header=FALSE, .delimiter='\t'};
  //  reader_t *reader = setup_reader("../../data/trace.csv", CSV_TRACE, OBJ_ID_STR, &init_params);

  sim_arg_t args = parse_cmd(argc, argv);

  if (args.debug) {
    printf("trace type %s, trace %s cache_size %ld MiB alg %s metadata_size %d, "
#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
           "seg size %d, n merge %d, rank_intvl %d, bucket type %d"
#endif
           "\n",
#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
        argv[1], args.trace_path, (long) args.cache_size, args.alg, args.per_obj_metadata,
           args.seg_size, args.n_merge, args.rank_intvl, args.bucket_type);
#else
    argv[1], args.trace_path, (long) args.cache_size, args.alg, args.per_obj_metadata);
#endif

    run_cache(args.reader, args.cache);
  } else {
    cache_stat_t *result = get_miss_ratio_curve(args.reader, args.cache, args.n_cache_size, args.cache_sizes,
                                             NULL, 0, args.n_thread);

    for (int i = 0; i < args.n_cache_size; i++) {
      printf("cache size %16" PRIu64 ": miss/n_req %16" PRIu64 "/%16" PRIu64 " (%.4lf), "
             "byte miss ratio %.4lf\n",
             result[i].cache_size, result[i].n_miss, result[i].n_req,
             (double) result[i].n_miss / (double) result[i].n_req,
             (double) result[i].n_miss_byte / (double) result[i].n_req_byte);
    }
  }

  close_reader(args.reader);
  cache_struct_free(args.cache);

  return 0;
}
