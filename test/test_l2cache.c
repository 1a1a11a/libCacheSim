//
// Created by Juncheng Yang on 11/21/19.
//

#include "common.h"

#define L2Cache_CACHE_SIZE (4 * GiB)
#define L2Cache_STEP_SIZE GiB

static void _verify_profiler_results(const cache_stat_t *res,
                              uint64_t num_of_sizes,
                              uint64_t req_cnt_true,
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

static void print_results(const cache_t *cache,
                         const cache_stat_t *res) {

  for (uint64_t i = 0; i < L2Cache_CACHE_SIZE / L2Cache_STEP_SIZE; i++) {
    printf("%s cache size %8.4lf GB, req %" PRIu64 " miss %8" PRIu64
           " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
           cache->cache_name,
           (double) res[i].cache_size / (double) GiB, 
           res[i].n_req, res[i].n_miss, res[i].n_req_byte,
           res[i].n_miss_byte);
  }
}

static void test_L2Cache_ORACLE_LOG(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  // uint64_t miss_cnt_true[] = {2076134, 1358640, 1048470, 945319};
  // uint64_t miss_cnt_true[] = {1800593, 1238324, 1016389, 949717};   // do not consider retain 
  uint64_t miss_cnt_true[] = {1843224, 1226081, 1069476, 942701};   // consider retain 


  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = L2Cache_CACHE_SIZE,
      .hashpower = 24, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-OracleLog", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, L2Cache_CACHE_SIZE / L2Cache_STEP_SIZE, req_cnt_true,
                           miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_L2Cache_ORACLE_ITEM(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {2573673, 1832838, 1555903, 1380232};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = L2Cache_CACHE_SIZE,
      .hashpower = 24, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-OracleItem", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, L2Cache_CACHE_SIZE / L2Cache_STEP_SIZE, req_cnt_true,
                           miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_L2Cache_ORACLE_BOTH(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {1485927, 1022736, 937075, 898857}; // consider retain
  // uint64_t miss_cnt_true[] = {1590088, 1137377, 956045, 912532};     // do not consider retain 

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = L2Cache_CACHE_SIZE,
      .hashpower = 24, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-OracleBoth", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, L2Cache_CACHE_SIZE / L2Cache_STEP_SIZE, req_cnt_true,
                           miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_L2Cache_LEARNED_TRUE_Y(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {2255891, 1601486, 1305691, 1205976};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = L2Cache_CACHE_SIZE,
      .hashpower = 24, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-LearnedTrueY", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, L2Cache_CACHE_SIZE / L2Cache_STEP_SIZE, req_cnt_true,
                           miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_L2Cache_LEARNED_ONLINE(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  // uint64_t miss_cnt_true[] = {2600178, 1598445, 1296337, 1212834};
  uint64_t miss_cnt_true[] = {2371604, 1590259, 1289356, 1205920};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = L2Cache_CACHE_SIZE,
      .hashpower = 24, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-LearnedOnline", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, L2Cache_CACHE_SIZE / L2Cache_STEP_SIZE, req_cnt_true,
                           miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_L2Cache_SEGCACHE(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {2867283, 2012928, 1691945, 1495364};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = L2Cache_CACHE_SIZE,
      .hashpower = 24, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-Segcache", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, L2Cache_CACHE_SIZE / L2Cache_STEP_SIZE, req_cnt_true,
                           miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void empty_test(gconstpointer user_data) { ; }

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  // where does the randomization come from, sort? 
  // the miss ratio results can vary a lot depending on the randomization
  srand(0); // for reproducibility
  reader_t *reader;

#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
  reader = setup_L2CacheTestData_reader(); 
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_LEARNED_TRUE_Y", reader, test_L2Cache_LEARNED_TRUE_Y);
  // g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_LEARNED_ONLINE", reader, test_L2Cache_LEARNED_ONLINE);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_ORACLE_LOG", reader, test_L2Cache_ORACLE_LOG);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_ORACLE_ITEM", reader, test_L2Cache_ORACLE_ITEM);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_ORACLE_BOTH", reader, test_L2Cache_ORACLE_BOTH);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_SEGCACHE", reader, test_L2Cache_SEGCACHE);


  g_test_add_data_func_full("/libCacheSim/empty", reader, empty_test,
                            test_teardown);
#endif /* ENABLE_L2CACHE */

  return g_test_run();
}
