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
  // uint64_t miss_cnt_true[] = {1816675, 1201297, 1059332, 961428};
  uint64_t miss_cnt_true[] = {1619051, 1077926, 946792, 898035};   // consider retain 


  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = L2Cache_CACHE_SIZE,
      .hashpower = 24, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-OracleLog", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, 0, _n_cores());
  print_results(cache, res); 
  _verify_profiler_results(res, L2Cache_CACHE_SIZE / L2Cache_STEP_SIZE, req_cnt_true,
                           miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_L2Cache_ORACLE_ITEM(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  // uint64_t miss_cnt_true[] = {2143656, 1438234, 1204795, 1081270};
  uint64_t miss_cnt_true[] = {2144860, 1463278, 1233928, 1093039};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = L2Cache_CACHE_SIZE,
      .hashpower = 24, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-OracleItem", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, 0, _n_cores());

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
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, L2Cache_CACHE_SIZE / L2Cache_STEP_SIZE, req_cnt_true,
                           miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_L2Cache_LEARNED_TRUE_Y(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {2276681, 1501129, 1253072, 1105797};
  // uint64_t miss_cnt_true[] = {2263035, 1509245, 1269869, 1176711};
  // uint64_t miss_cnt_true[] = {2428542, 1521798, 1286571, 1189299};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = L2Cache_CACHE_SIZE,
      .hashpower = 24, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-LearnedTrueY", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, L2Cache_CACHE_SIZE / L2Cache_STEP_SIZE, req_cnt_true,
                           miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_L2Cache_LEARNED_ONLINE(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {2337883, 1595886, 1278501, 1139863};
  // uint64_t miss_cnt_true[] = {2371604, 1590259, 1289356, 1205920};
  // uint64_t miss_cnt_true[] = {2662530, 1600746, 1319404, 1254072};    // cannot decrease because of too large segments? 

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = L2Cache_CACHE_SIZE,
      .hashpower = 24, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-LearnedOnline", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, L2Cache_CACHE_SIZE / L2Cache_STEP_SIZE, req_cnt_true,
                           miss_cnt_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_L2Cache_SEGCACHE(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {2774838, 1937879, 1627133, 1432318};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = L2Cache_CACHE_SIZE,
      .hashpower = 24, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-Segcache", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, L2Cache_STEP_SIZE, NULL, 0, 0, _n_cores());

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
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_LEARNED_ONLINE", reader, test_L2Cache_LEARNED_ONLINE);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_ORACLE_LOG", reader, test_L2Cache_ORACLE_LOG);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_ORACLE_ITEM", reader, test_L2Cache_ORACLE_ITEM);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_ORACLE_BOTH", reader, test_L2Cache_ORACLE_BOTH);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_SEGCACHE", reader, test_L2Cache_SEGCACHE);


  g_test_add_data_func_full("/libCacheSim/empty", reader, empty_test,
                            test_teardown);
#endif /* ENABLE_L2CACHE */

  return g_test_run();
}
