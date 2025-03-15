#include <libCacheSim.h>

#include <thread>

#define NUM_SIZES 8
const char *TRACE_PATH = "../../../data/cloudPhysicsIO.csv";

void run_one_cache_multiple_sizes(cache_t *cache, reader_t *reader) {
  uint64_t cache_sizes[NUM_SIZES] = {100,  200,  400,  800,
                                     1600, 3200, 6400, 12800};
  for (int i = 0; i < NUM_SIZES; i++) {
    cache_sizes[i] = cache_sizes[i] * MiB;
  }

  /* see libCacheSim/include/simulator.h
     run several concurrent simulations with different cache sizes
     parameters: reader, cache, cache size, num_sizes, cache_sizes,
     warmup_reader, warmup_frac, warmup_sec, num_threads
   */
  cache_stat_t *result = simulate_at_multi_sizes(
      reader, cache, NUM_SIZES, cache_sizes, nullptr, 0.0, 0,
      static_cast<int>(std::thread::hardware_concurrency()), false);

  printf(
      "      cache name        cache size           num_miss        num_req"
      "        miss ratio      byte miss ratio\n");
  for (int i = 0; i < NUM_SIZES; i++) {
    printf("%16s %16lu %16lu %16lu %16.4lf %16.4lf\n", result[i].cache_name,
           result[i].cache_size, result[i].n_miss, result[i].n_req,
           (double)(result[i].n_miss) / (double)result[i].n_req,
           (double)(result[i].n_miss_byte / (double)result[i].n_req_byte));
  }

  cache->cache_free(cache);
  free(result);
}

void run_multiple_caches(reader_t *reader) {
  reset_reader(reader);

  common_cache_params_t cc_params = default_common_cache_params();
  cc_params.cache_size = 1 * GiB;
  cc_params.hashpower = 16;

  cache_t *caches[8] = {
      LRU_init(cc_params, nullptr),  LFU_init(cc_params, nullptr),
      FIFO_init(cc_params, nullptr), Sieve_init(cc_params, nullptr),
      LHD_init(cc_params, nullptr),  LeCaR_init(cc_params, nullptr),
      ARC_init(cc_params, nullptr),  NULL};

  // Hyperbolic samples eviction candidates from the cache,
  // so the hash table needs to be small
  cc_params.hashpower = 12;
  caches[7] = Hyperbolic_init(cc_params, nullptr);

  cache_stat_t *result = simulate_with_multi_caches(
      reader, caches, 8, nullptr, 0.0, 0,
      static_cast<int>(std::thread::hardware_concurrency()), 0, false);

  printf(
      "      cache name        cache size           num_miss        num_req"
      "        miss ratio      byte miss ratio\n");
  for (int i = 0; i < NUM_SIZES; i++) {
    printf("%16s %16lu %16lu %16lu %16.4lf %16.4lf\n", result[i].cache_name,
           result[i].cache_size, result[i].n_miss, result[i].n_req,
           (double)(result[i].n_miss) / (double)result[i].n_req,
           (double)(result[i].n_miss_byte / (double)result[i].n_req_byte));
  }
  free(result);
  for (int i = 0; i < 8; i++) {
    caches[i]->cache_free(caches[i]);
  }
}

int main(int argc, char *argv[]) {
  reader_init_param_t init_params = default_reader_init_params();
  /* the first column is the time, the second is the object id, the third is the
   * object size */
  init_params.time_field = 2;
  init_params.obj_id_field = 5;
  init_params.obj_size_field = 4;

  /* the trace has a header */
  init_params.has_header = true;
  init_params.has_header_set = true;

  /* the trace uses comma as the delimiter */
  init_params.delimiter = ',';

  /* object id in the trace is numeric */
  init_params.obj_id_is_num = true;

  reader_t *reader = open_trace(TRACE_PATH, CSV_TRACE, &init_params);

  common_cache_params_t cc_params = default_common_cache_params();
  cc_params.cache_size = 1 * GiB;  // any size should work
  cache_t *cache = LRU_init(cc_params, nullptr);

  printf("==== run one cache at different sizes ====\n");
  run_one_cache_multiple_sizes(cache, reader);

  /* a different example of concurrent simulations */
  printf("==== run multiple caches ====\n");
  run_multiple_caches(reader);

  close_trace(reader);
}
