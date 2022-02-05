//
// Created by Juncheng Yang on 11/21/19.
//

#include "common.h"

static void _verify_profiler_results(const cache_stat_t *res,
                              uint64_t num_of_sizes,
                              uint64_t req_cnt_true,
                              const uint64_t *miss_cnt_true,
                              uint64_t req_byte_true,
                              const uint64_t *miss_byte_true) {

  for (uint64_t i = 0; i < num_of_sizes; i++) {
    g_assert_cmpuint(req_cnt_true, ==, res[i].n_req);
    g_assert_cmpfloat((double)(res[i].n_miss - miss_cnt_true[i]) / miss_cnt_true[i], <=, 0.01);
    g_assert_cmpuint(req_byte_true, ==, res[i].n_req_byte);
    g_assert_cmpfloat((double)(res[i].n_miss_byte - miss_byte_true[i]) / miss_byte_true[i], <=, 0.01);
  }
}

static void print_results(const cache_t *cache,
                         const cache_stat_t *res) {

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf("%s cache size %16" PRIu64 " req %" PRIu64 " miss %8" PRIu64
           " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
           cache->cache_name,
           res[i].cache_size, res[i].n_req, res[i].n_miss, res[i].n_req_byte,
           res[i].n_miss_byte);
  }
}

static void test_L2Cache_ORACLE_LOG(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {6459134, 4849425, 3960033, 3185497, 2715623, 2536562, 2199445, 2071379};
  uint64_t miss_byte_true[] = {124680727040, 103748910080, 90579069440, 79519862784, 70003931136, 65120653824, 59818039808, 54074082816};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-OracleLog", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}
static void test_L2Cache_ORACLE_ITEM(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {6389915, 5224625, 4334381, 3747497, 3323236, 2992171, 2758805, 2574795};
  uint64_t miss_byte_true[] = {122192135168, 103598505472, 88274618368, 77836705280, 70164604928, 64013555200, 59667272704, 56197281280};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-OracleItem", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}
static void test_L2Cache_ORACLE_BOTH(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {6131542, 5251245, 4561764, 3595778, 3062567, 2789390, 2341065, 1934686};
  uint64_t miss_byte_true[] = {126390494208, 117896260096, 110056763392, 100195554304, 93243265024, 88165374464, 79403701760, 70925711872};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-OracleBoth", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}
static void test_L2Cache_LEARNED_TRUE_Y(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {6751007, 5699593, 4841244, 4171326, 3634421, 3262314, 2991878, 2778794};
  uint64_t miss_byte_true[] = {129173642240, 111849560576, 97042920448, 84940804096, 75040174592, 68449256960, 63475947520, 59417136640};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-LearnedTrueY", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}
static void test_L2Cache_LEARNED_ONLINE(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {6751007, 5699593, 4841244, 4171326, 3634421, 3262314, 2991878, 2778794};
  uint64_t miss_byte_true[] = {129173642240, 111849560576, 97042920448, 84940804096, 75040174592, 68449256960, 63475947520, 59417136640};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-LearnedOnline", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}
static void test_L2Cache_SEGCACHE(gconstpointer user_data) {
  uint64_t req_cnt_true = 8875971, req_byte_true = 160011631104;
  uint64_t miss_cnt_true[] = {6751007, 5699593, 4841244, 4171326, 3634421, 3262314, 2991878, 2778794};
  uint64_t miss_byte_true[] = {129173642240, 111849560576, 97042920448, 84940804096, 75040174592, 68449256960, 63475947520, 59417136640};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  
  cache_t *cache = create_test_cache("L2Cache-Segcache", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, _n_cores());

  print_results(cache, res); 
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void empty_test(gconstpointer user_data) { ; }

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t *reader;

#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
  reader = setup_L2CacheTestData_reader(); 
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_ORACLE_LOG", reader, test_L2Cache_ORACLE_LOG);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_ORACLE_ITEM", reader, test_L2Cache_ORACLE_ITEM);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_ORACLE_BOTH", reader, test_L2Cache_ORACLE_BOTH);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_LEARNED_TRUE_Y", reader, test_L2Cache_LEARNED_TRUE_Y);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_LEARNED_ONLINE", reader, test_L2Cache_LEARNED_ONLINE);
  g_test_add_data_func("/libCacheSim/cacheAlgo_L2Cache_SEGCACHE", reader, test_L2Cache_SEGCACHE);


  g_test_add_data_func_full("/libCacheSim/empty", reader, empty_test,
                            test_teardown);
#endif /* ENABLE_L2CACHE */

  return g_test_run();
}
