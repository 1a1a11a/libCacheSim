//
// Created by Haocheng Xia on 02/11/25.
//

#include "common.h"

#define ThreeLCache_CACHE_SIZE (4 * GiB)
#define ThreeLCache_STEP_SIZE GiB

static void _verify_profiler_results(const cache_stat_t *res, uint64_t num_of_sizes, uint64_t req_cnt_true,
                                     const uint64_t *miss_cnt_true) {
  for (uint64_t i = 0; i < num_of_sizes; i++) {
    g_assert_cmpuint(req_cnt_true, ==, res[i].n_req);
    double true_v = miss_cnt_true[i];
    double res_v = res[i].n_miss;
    double diff = fabs(res_v - true_v) / true_v;
    if (diff > 0.02) {
      printf("true %.0lf curr %.0lf %lf\n", true_v, res_v, diff);
      g_assert(0);
    }
  }
}

static void print_results(const cache_t *cache, const cache_stat_t *res) {
  for (uint64_t i = 0; i < ThreeLCache_CACHE_SIZE / ThreeLCache_STEP_SIZE; i++) {
    printf("%s cache size %8.4lf GB, req %" PRIu64 " miss %8" PRIu64 " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
           cache->cache_name, (double)res[i].cache_size / (double)GiB, res[i].n_req, res[i].n_miss, res[i].n_req_byte,
           res[i].n_miss_byte);
  }
}

static void test_3LCache_OBJECT_MISS_RATIO(gconstpointer user_data) {
  uint64_t req_cnt_true = 13625211, req_byte_true = 187055050752;
  uint64_t miss_cnt_true[] = {4779140, 4619176, 4099469, 4070435};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = ThreeLCache_CACHE_SIZE, .hashpower = 24, .default_ttl = DEFAULT_TTL};

  cache_t *cache = create_test_cache("3LCache-object-miss-ratio", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, ThreeLCache_STEP_SIZE, NULL, 0, 0, _n_cores(), false);
  print_results(cache, res);
  _verify_profiler_results(res, ThreeLCache_CACHE_SIZE / ThreeLCache_STEP_SIZE, req_cnt_true, miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_3LCache_BYTE_MISS_RATIO(gconstpointer user_data) {
  uint64_t req_cnt_true = 13625211, req_byte_true = 187055050752;

  uint64_t miss_cnt_true[] = {4821107, 4476524, 4374172, 4350448};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = ThreeLCache_CACHE_SIZE, .hashpower = 24, .default_ttl = DEFAULT_TTL};

  cache_t *cache = create_test_cache("3LCache-byte-miss-ratio", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, ThreeLCache_STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, ThreeLCache_CACHE_SIZE / ThreeLCache_STEP_SIZE, req_cnt_true, miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void empty_test(gconstpointer user_data) { ; }

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  srand(0);  // for reproducibility
  reader_t *reader;

#if defined(ENABLE_3L_CACHE) && ENABLE_3L_CACHE == 1
  reader = setup_3LCacheTestData_reader();
  g_test_add_data_func("/libCacheSim/cacheAlgo_3LCache_OBJECT_MISS_RATIO", reader, test_3LCache_OBJECT_MISS_RATIO);
  g_test_add_data_func("/libCacheSim/cacheAlgo_3LCache_BYTE_MISS_RATIO", reader, test_3LCache_BYTE_MISS_RATIO);

  g_test_add_data_func_full("/libCacheSim/empty", reader, empty_test, test_teardown);
#endif /* ENABLE_3L_CACHE */

  return g_test_run();
}
