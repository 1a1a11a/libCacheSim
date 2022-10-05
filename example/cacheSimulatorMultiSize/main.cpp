#include <libCacheSim.h>

#include <thread>

#define NUM_SIZES 8

int main(int argc, char *argv[]) {
  uint64_t cache_sizes[8] = {100, 200, 400, 800, 1600, 3200, 6400, 12800};
  for (int i = 0; i < NUM_SIZES; i++) {
    cache_sizes[i] = cache_sizes[i] * MiB;
  }

  reader_init_param_t init_params;
  set_default_reader_init_params(&init_params);
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

  reader_t *reader =
      open_trace("../../../data/trace.csv", CSV_TRACE, &init_params);

  common_cache_params_t cc_params;
  cc_params.cache_size = 1 * GiB;  // any size should work
  cache_t *cache = LHD_init(cc_params, nullptr);

  /* see libCacheSim/include/simulator.h
     run several concurrent simulations with different cache sizes
     parameters: reader, cache, cache size, num_sizes, cache_sizes,
     warmup_reader, warmup_frac, warmup_sec, num_threads
   */
  cache_stat_t *result = simulate_at_multi_sizes(
      reader, cache, NUM_SIZES, cache_sizes, nullptr, 0.0, 0,
      static_cast<int>(std::thread::hardware_concurrency()));

  printf(
      "      cache size           num_miss        num_req        miss ratio    "
      "  byte miss ratio\n");
  for (int i = 0; i < NUM_SIZES; i++) {
    printf("%16lu %16lu %16lu %16.4lf %16.4lf\n", result[i].cache_size,
           result[i].n_miss, result[i].n_req,
           (double)(result[i].n_miss) / (double)result[i].n_req,
           (double)(result[i].n_miss_byte / (double)result[i].n_req_byte));
  }

  close_trace(reader);
  cache->cache_free(cache);
  free(result);
}
