

#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <assert.h>

#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/simulator.h"

#include "cachesim.h"
#include "priv/params.h"
#include "utils.h"


void run_cache_debug(reader_t *reader, cache_t *cache) {
  request_t *req = new_request();
  uint64_t req_cnt = 0, miss_cnt = 0;
  uint64_t req_byte = 0, miss_byte = 0;

  read_one_req(reader, req);
  int64_t start_ts = (int64_t) req->real_time, last_report_ts;

  /* skip half of the requests to warm up */
  long n_skipped = 0;
  for (int i = 0; i < reader->n_total_req / 2; i++) {
    n_skipped += 1;
    req->real_time -= start_ts;
    cache->get(cache, req);
    read_one_req(reader, req);
  }
  // printf("%.2lf hour: skip %ld requests\n", 
  //     (double) (req->real_time - start_ts) / 3600.0, n_skipped);
  last_report_ts = req->real_time - start_ts;

  double start_time = gettime();
  while (req->valid) {
    req_cnt++;
    req_byte += req->obj_size;
    req->real_time -= start_ts;
    if (cache->get(cache, req) != cache_ck_hit) {
      miss_cnt++;
      miss_byte += req->obj_size;
    }

    if (req->real_time - last_report_ts >= 3600 * 24 && req->real_time != 0) {
      INFO("%.2lf hour: %lu requests, miss ratio %.4lf\n",
           (double) req->real_time / 3600, 
           (unsigned long) req_cnt, 
          //  (unsigned long) miss_cnt,
           (double) miss_cnt / req_cnt);

#ifdef TRACK_EVICTION_AGE
      print_eviction_age(cache);
#endif
      last_report_ts = (int64_t) req->real_time;
    }

    read_one_req(reader, req);
  }

  double runtime = gettime() - start_time;
  // printf("runtime %lf s\n", runtime);
  INFO("%.2lf hour: %lu requests, miss ratio %.4lf, miss byte %.4lf,"
       " throughput %.2lf MQPS, skipped %ld requests\n",
       (double) req->real_time / 3600.0, (unsigned long) req_cnt,
       (double) miss_cnt / req_cnt,
       (double) miss_byte/req_byte,
       (double) req_cnt / 1000000.0 / runtime,
       n_skipped);
}

int main(int argc, char **argv) {

  sim_arg_t args = parse_cmd(argc, argv);

  if (args.debug) {
    printf("trace type %s, trace %s cache_size %ld MiB alg %s metadata_size %d\n",
      argv[1], args.trace_path, (long) args.cache_size, args.alg, args.per_obj_metadata);

#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
    printf("seg size %d, n merge %d, rank_intvl %.4lf, bucket type %d\n", 
      args.seg_size, args.n_merge, args.rank_intvl, args.bucket_type); 
#endif 

    run_cache_debug(args.reader, args.cache);
  } else {
    cache_stat_t *result = get_miss_ratio_curve(args.reader, 
                                                args.cache, 
                                                args.n_cache_size, 
                                                args.cache_sizes,
                                                NULL, 0, args.n_thread);

    char output_str[1024]; 
    char output_filename[128]; 
    create_dir("result/"); 
    sprintf(output_filename, "result/%s", rindex(args.trace_path, '/') + 1);
    FILE *output_file = fopen(output_filename, "a");
    for (int i = 0; i < args.n_cache_size; i++) {
      snprintf(output_str, 1024, 
             "%s %s, cache size %16" PRIu64 ": miss/n_req %16" PRIu64 "/%16" PRIu64 " (%.4lf), "
             "byte miss ratio %.4lf\n",
             output_filename, args.alg, result[i].cache_size, result[i].n_miss, result[i].n_req,
             (double) result[i].n_miss / (double) result[i].n_req,
             (double) result[i].n_miss_byte / (double) result[i].n_req_byte); 
      printf("%s", output_str);
      fprintf(output_file, "%s", output_str);
    }
    fclose(output_file); 
  }

  close_reader(args.reader);
  cache_struct_free(args.cache);

  return 0;
}
