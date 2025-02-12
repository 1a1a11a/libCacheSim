//
// Created by Juncheng Yang on 11/21/19.
//

#include "common.h"

#define GLCache_CACHE_SIZE (4 * GiB)
#define GLCache_STEP_SIZE GiB

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
  for (uint64_t i = 0; i < GLCache_CACHE_SIZE / GLCache_STEP_SIZE; i++) {
    printf("%s cache size %8.4lf GB, req %" PRIu64 " miss %8" PRIu64 " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
           cache->cache_name, (double)res[i].cache_size / (double)GiB, res[i].n_req, res[i].n_miss, res[i].n_req_byte,
           res[i].n_miss_byte);
  }
}

static void test_GLCache_ORACLE_LOG(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {1630908, 1126212, 979809, 922256};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = GLCache_CACHE_SIZE, .hashpower = 24, .default_ttl = DEFAULT_TTL};

  cache_t *cache = create_test_cache("GLCache-OracleLog", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, GLCache_STEP_SIZE, NULL, 0, 0, _n_cores(), false);
  print_results(cache, res);
  _verify_profiler_results(res, GLCache_CACHE_SIZE / GLCache_STEP_SIZE, req_cnt_true, miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_GLCache_ORACLE_ITEM(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {1966381, 1310825, 1198366, 1127507};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = GLCache_CACHE_SIZE, .hashpower = 24, .default_ttl = DEFAULT_TTL};

  cache_t *cache = create_test_cache("GLCache-OracleItem", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, GLCache_STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, GLCache_CACHE_SIZE / GLCache_STEP_SIZE, req_cnt_true, miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_GLCache_ORACLE_BOTH(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {1476768, 1091173, 1027426, 1004396};  // do not consider retain

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = GLCache_CACHE_SIZE, .hashpower = 24, .default_ttl = DEFAULT_TTL};

  cache_t *cache = create_test_cache("GLCache-OracleBoth", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, GLCache_STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, GLCache_CACHE_SIZE / GLCache_STEP_SIZE, req_cnt_true, miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_GLCache_LEARNED_TRUE_Y(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {2021753, 1314854, 1093074, 1034222};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = GLCache_CACHE_SIZE, .hashpower = 24, .default_ttl = DEFAULT_TTL};

  cache_t *cache = create_test_cache("GLCache-LearnedTrueY", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, GLCache_STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, GLCache_CACHE_SIZE / GLCache_STEP_SIZE, req_cnt_true, miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_GLCache_LEARNED_ONLINE(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;

  uint64_t miss_cnt_true[] = {2187591, 1375970, 1113121, 1046752};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = GLCache_CACHE_SIZE, .hashpower = 24, .default_ttl = DEFAULT_TTL};

  cache_t *cache = create_test_cache("GLCache-LearnedOnline", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, GLCache_STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, GLCache_CACHE_SIZE / GLCache_STEP_SIZE, req_cnt_true, miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void empty_test(gconstpointer user_data) { ; }

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  srand(0);  // for reproducibility
  reader_t *reader;

#if defined(ENABLE_GLCACHE) && ENABLE_GLCACHE == 1
  reader = setup_GLCacheTestData_reader();
  g_test_add_data_func("/libCacheSim/cacheAlgo_GLCache_LEARNED_TRUE_Y", reader, test_GLCache_LEARNED_TRUE_Y);
  g_test_add_data_func("/libCacheSim/cacheAlgo_GLCache_LEARNED_ONLINE", reader, test_GLCache_LEARNED_ONLINE);
  g_test_add_data_func("/libCacheSim/cacheAlgo_GLCache_ORACLE_LOG", reader, test_GLCache_ORACLE_LOG);
  g_test_add_data_func("/libCacheSim/cacheAlgo_GLCache_ORACLE_ITEM", reader, test_GLCache_ORACLE_ITEM);
  g_test_add_data_func("/libCacheSim/cacheAlgo_GLCache_ORACLE_BOTH", reader, test_GLCache_ORACLE_BOTH);

  g_test_add_data_func_full("/libCacheSim/empty", reader, empty_test, test_teardown);
#endif /* ENABLE_GLCACHE */

  return g_test_run();
}
