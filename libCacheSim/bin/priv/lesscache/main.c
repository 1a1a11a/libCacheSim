/**
 * this is used to run the simulations in lesscache
 *
 **/

#include "../../../include/libCacheSim/cache.h"
#include "../../../include/libCacheSim/evictionAlgo.h"
#include "../../../include/libCacheSim/reader.h"
#include "../../../include/libCacheSim/simulator.h"
#include "../../../utils/include/mymath.h"
#include "../../../utils/include/mysys.h"

// #define ADD_METADATA
// #define NO_FRAGMENTATION
// #define NO_CALCIFICATION
// #define FIFO_REDUCE_METADATA
// #define SAMPLING_RATIO 100   // 1 / SAMPLING_RATIO


#ifdef NO_FRAGMENTATION
static int alloc_sizes[47] = {
    72,     96,     120,    152,     192,     240,     304,    384,
    480,    600,    752,    944,     1184,    1480,    1856,   2320,
    2904,   3632,   4544,   5680,    7104,    8880,    11104,  13880,
    17352,  21696,  27120,  33904,   42384,   52984,   66232,  82792,
    103496, 129376, 161720, 202152,  252696,  315872,  394840, 493552,
    616944, 771184, 963984, 1204984, 1506232, 1882792, 4194304};

static int size_mapping[1048576];

#endif  // NO_FRAGMENTATION


void run_cache(reader_t *reader, cache_t *cache) {
  request_t *req = new_request();
  uint64_t req_cnt = 0, miss_cnt = 0;
  uint64_t last_req_cnt = 0, last_miss_cnt = 0;

  read_one_req(reader, req);
  uint64_t start_ts = (uint64_t)req->clock_time;
  uint64_t last_report_ts = req->clock_time - start_ts;

  double start_time = gettime();
  while (req->valid) {
    req->clock_time -= start_ts;

#ifdef FIFO_REDUCE_METADATA
    req->obj_size -= 12;
#endif // FIFO_REDUCE_METADATA
#ifdef ADD_METADATA
    req->obj_size += 32;
#endif  // ADD_METADATA

#ifdef NO_FRAGMENTATION
#ifdef NO_CALCIFICATION
#ifndef ADD_METADATA
#error "ERROR: NO_CALCIFICATION requires ADD_METADATA\n"
#endif // ADD_METADATA
    // value + key + metadata NO_CALCIFICATION
    req->obj_size = size_mapping[160 + 24 + 32];
#else // NO NO_CALCIFICATION
    // object size = value + key
    req->obj_size = size_mapping[req->obj_size];
#endif // NO_CALCIFICATION
#endif // NO_FRAGMENTATION

#ifdef SAMPLING_RATIO
    if (req->obj_id % (SAMPLING_RATIO * 100 + 1) > 100) {
         read_one_req(reader, req);
          continue;
    }
#endif // SAMPLING_RATIO

    if (req->op == OP_SET || req->op == OP_REPLACE) {
      cache->get(cache, req);
    } else if (req->op == OP_DELETE) {
      cache->remove(cache, req->obj_id);
    } else {
      req_cnt++;
      if (cache->get(cache, req) == false) {
        miss_cnt++;
      }
    }

    if (req->clock_time - last_report_ts >= 3600 * 24 && req->clock_time != 0) {
      INFO(
          "%.2lf hour: %lu requests, miss ratio %.4lf, interval miss ratio "
          "%.4lf\n",
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
  INFO(
      "%.2lf hour: %s, cache size %d, %lu "
      "requests, miss ratio %.4lf, "
      "throughput %.2lf MQPS\n",
      (double)req->clock_time / 3600.0, reader->trace_path,
      (int)(cache->cache_size / 1024 / 1024), (unsigned long)req_cnt,
      (double)miss_cnt / req_cnt,
      //  (double) miss_byte/req_byte,
      (double)req_cnt / 1000000.0 / runtime);
}

int main(int argc, char **argv) {
  srand(time(NULL));
  set_rand_seed(rand());

  if (argc < 3) {
    printf("Usage: %s trace_path eviction cache_size", argv[0]);
    return 0;
  }

  char *trace_path = argv[1];
  char *eviction_algo = argv[2];
  int cache_size_in_mb = atoi(argv[3]);

  reader_t *reader =
      setup_reader(trace_path, ORACLE_SYS_TWRNS_TRACE, NULL);

  common_cache_params_t cc_params = {.cache_size = cache_size_in_mb * MiB,
                                     .hashpower = 28,
                                     .default_ttl = 86400 * 300,
                                     .consider_obj_metadata = false};
  cache_t *cache;

#ifdef SAMPLING_RATIO
  cc_params.cache_size /= SAMPLING_RATIO;
#endif // SAMPLING_RATIO

  if (strcasecmp(eviction_algo, "lru") == 0) {
    cache = LRU_init(cc_params, NULL);
  } else if (strcasecmp(eviction_algo, "lhd") == 0) {
    cache = LHD_init(cc_params, NULL);
  } else if (strcasecmp(eviction_algo, "fifo") == 0) {
    cache = FIFO_init(cc_params, NULL);
  } else {
    printf("unknown eviction %s\n", eviction_algo);
    exit(1);
  }

#ifdef NO_FRAGMENTATION
  int pos = 0;
  for (int i = 0; i < 1048576; i++) {
    if (i > alloc_sizes[pos]) {
      pos += 1;
    }
    size_mapping[i] = alloc_sizes[pos];
  }
#endif  // NO_FRAGMENTATION

  run_cache(reader, cache);

  close_reader(reader);
  cache->cache_free(cache);

  return 0;
}
