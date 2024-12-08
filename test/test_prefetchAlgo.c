//
// Created by Zhelong Zhao on 2023.08.16.
//
#include "../libCacheSim/utils/include/mymath.h"
#include "common.h"

static const uint64_t g_req_cnt_true = 113872, g_req_byte_true = 4368040448;

static void _verify_profiler_results(const cache_stat_t *res, uint64_t num_of_sizes, uint64_t req_cnt_true,
                                     const uint64_t *miss_cnt_true, uint64_t req_byte_true,
                                     const uint64_t *miss_byte_true) {
  for (uint64_t i = 0; i < num_of_sizes; i++) {
    g_assert_cmpuint(req_cnt_true, ==, res[i].n_req);
    g_assert_cmpuint(miss_cnt_true[i], ==, res[i].n_miss);
    g_assert_cmpuint(req_byte_true, ==, res[i].n_req_byte);
    g_assert_cmpuint(miss_byte_true[i], ==, res[i].n_miss_byte);
  }
}

static void print_results(const cache_t *cache, const cache_stat_t *res) {
  printf("%s uint64_t cache_size[] = {", cache->cache_name);
  printf("%ld", (long)res[0].cache_size);
  for (uint64_t i = 1; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf(", %ld", (long)res[i].cache_size);
  }
  printf("};\n");

  printf("uint64_t miss_cnt_true[] = {");
  printf("%ld", (long)res[0].n_miss);
  for (uint64_t i = 1; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf(", %ld", (long)res[i].n_miss);
  }
  printf("};\n");

  printf("uint64_t miss_byte_true[] = {");
  printf("%ld", (long)res[0].n_miss_byte);
  for (uint64_t i = 1; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf(", %ld", (long)res[i].n_miss_byte);
  }
  printf("};\n");
}

static void test_Mithril(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {79796, 78480, 76126, 75256, 72336, 72062, 71936, 71667};
  uint64_t miss_byte_true[] = {3471357440, 3399726080, 3285093888, 3245231616,
                               3092759040, 3077801472, 3075234816, 3061489664};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Mithril", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true, miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_OBL(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {92139, 88548, 82337, 80487, 71259, 70869, 70737, 70469};
  uint64_t miss_byte_true[] = {4213140480, 4060079616, 3776877568, 3659406848,
                               3099764736, 3076965888, 3074241024, 3060499968};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("OBL", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true, miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_PG(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {92786, 89494, 83403, 81564, 72360, 71973, 71842, 71574};
  uint64_t miss_byte_true[] = {4195964416, 4054977024, 3776220672, 3659069952,
                               3100251136, 3077595648, 3074874880, 3061133824};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("PG", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true, miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  srand(0);  // for reproducibility
  set_rand_seed(rand());

  reader_t *reader;

  // do not use these two because object size change over time and
  // not all algorithms can handle the object size change correctly
  // reader = setup_csv_reader_obj_num();
  // reader = setup_vscsi_reader();

  reader = setup_oracleGeneralBin_reader();
  // reader = setup_vscsi_reader_with_ignored_obj_size();
  g_test_add_data_func("/libCacheSim/cacheAlgo_Mithril", reader, test_Mithril);
  g_test_add_data_func("/libCacheSim/cacheAlgo_OBL", reader, test_OBL);
  g_test_add_data_func("/libCacheSim/cacheAlgo_PG", reader, test_PG);

  return g_test_run();
}