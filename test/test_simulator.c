//
// Created by Juncheng Yang on 11/21/19.
//

#include "common.h"

/**
 * this one for testing with the plain trace reader, which does not have obj
 * size information
 * @param user_data
 */
static void test_simulator_no_size(gconstpointer user_data) {
  uint64_t cache_size = CACHE_SIZE / CACHE_SIZE_UNIT;
  uint64_t step_size = STEP_SIZE / CACHE_SIZE_UNIT;

  uint64_t req_cnt_true = 113872;
  uint64_t miss_cnt_true[] = {99411, 96397, 95652, 95370,
                              95182, 94997, 94891, 94816};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = cache_size,
                                     .default_ttl = 0};
  cache_t *cache = LRU_init(cc_params, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, step_size, NULL, 0, 0, _n_cores());

  //  uint64_t* mc = _get_lru_miss_cnt(reader, get_num_of_req(reader));

  for (uint64_t i = 0; i < cache_size / step_size; i++) {
    g_assert_cmpuint(res[i].cache_size, ==, step_size * (i + 1));
    g_assert_cmpuint(req_cnt_true, ==, res[i].n_req);
    g_assert_cmpuint(miss_cnt_true[i], ==, res[i].n_miss);
    g_assert_cmpuint(req_cnt_true, ==, res[i].n_req_byte);
    g_assert_cmpuint(miss_cnt_true[i], ==, res[i].n_miss_byte);
  }
  cache->cache_free(cache);
  g_free(res);
}

/**
 * this one for testing with all other cache readers, which contain obj size
 * information
 * @param user_data
 */
static void test_simulator(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {93161, 87794, 82945, 81433,
                              72250, 72083, 71969, 71716};
  uint64_t miss_byte_true[] = {4036642816, 3838227968, 3664065536, 3611544064,
                               3081624576, 3079594496, 3075357184, 3060711936};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
                                     .default_ttl = 0,
                                     .hashpower = 16,
                                     .consider_obj_metadata = false};
  cache_t *cache = LRU_init(cc_params, NULL);
  g_assert_true(cache != NULL);

  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());
  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    g_assert_cmpuint(res[i].cache_size, ==, STEP_SIZE * (i + 1));
    g_assert_cmpuint(res[i].n_req, ==, req_cnt_true);
    g_assert_cmpuint(res[i].n_req_byte, ==, req_byte_true);
    g_assert_cmpuint(res[i].n_miss, ==, miss_cnt_true[i]);
    g_assert_cmpuint(res[i].n_miss_byte, ==, miss_byte_true[i]);
  }
  g_free(res);

  uint64_t cache_sizes[] = {STEP_SIZE, STEP_SIZE * 2, STEP_SIZE * 4,
                            STEP_SIZE * 7};
  res = simulate_at_multi_sizes(reader, cache, 4, cache_sizes, NULL, 0, 0,
                                _n_cores());
  g_assert_cmpuint(res[0].cache_size, ==, STEP_SIZE);
  g_assert_cmpuint(res[1].n_req_byte, ==, req_byte_true);
  g_assert_cmpuint(res[3].n_req, ==, req_cnt_true);
  g_assert_cmpuint(res[0].n_miss_byte, ==, miss_byte_true[0]);
  g_assert_cmpuint(res[2].n_miss, ==, miss_cnt_true[3]);
  g_assert_cmpuint(res[3].n_miss_byte, ==, miss_byte_true[6]);
  g_free(res);

  cache->cache_free(cache);

  cache_t *caches[4];
  for (int i = 0; i < 4; i++) {
    cc_params.cache_size = cache_sizes[i];
    caches[i] = LRU_init(cc_params, NULL);
    g_assert_true(caches[i] != NULL);
  }

  res = simulate_with_multi_caches(reader, caches, 4, NULL, 0, 0, _n_cores());
  g_assert_cmpuint(res[0].cache_size, ==, STEP_SIZE);
  g_assert_cmpuint(res[1].n_req_byte, ==, req_byte_true);
  g_assert_cmpuint(res[3].n_req, ==, req_cnt_true);
  g_assert_cmpuint(res[0].n_miss_byte, ==, miss_byte_true[0]);
  g_assert_cmpuint(res[2].n_miss, ==, miss_cnt_true[3]);
  g_assert_cmpuint(res[3].n_miss_byte, ==, miss_byte_true[6]);
  g_free(res);

  for (int i = 0; i < 4; i++) {
    caches[i]->cache_free(caches[i]);
  }
}

/**
 * this one for testing warmup
 * @param user_data
 */
static void test_simulator_with_warmup1(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {93009, 87633, 82782, 81267,
                              72080, 71911, 71770, 71333};
  uint64_t miss_byte_true[] = {4034876416, 3836408320, 3662237696, 3609691648,
                               3079616512, 3077579776, 3072181760, 3045314048};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
                                     .default_ttl = 0};
  cache_t *cache = LRU_init(cc_params, NULL);
  g_assert_true(cache != NULL);

  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, reader, 0, 0, _n_cores());

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    g_assert_cmpuint(res[i].cache_size, ==, STEP_SIZE * (i + 1));
    g_assert_cmpuint(res[i].n_req, ==, req_cnt_true);
    g_assert_cmpuint(res[i].n_miss, ==, miss_cnt_true[i]);
    g_assert_cmpuint(res[i].n_req_byte, ==, req_byte_true);
    g_assert_cmpuint(res[i].n_miss_byte, ==, miss_byte_true[i]);
  }
  g_free(res);

  cache->cache_free(cache);
}

static void test_simulator_with_warmup2(gconstpointer user_data) {
  uint64_t req_cnt_true = 91098, req_byte_true = 3180282368;
  uint64_t miss_cnt_true[] = {75028, 69710, 65084, 63574,
                              57253, 57101, 56987, 56734};
  uint64_t miss_byte_true[] = {3036331008, 2839400448, 2676338688, 2623825408,
                               2263264256, 2261316096, 2257078784, 2242433536};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
                                     .default_ttl = 0};
  cache_t *cache = LRU_init(cc_params, NULL);
  g_assert_true(cache != NULL);

  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0.2, 0, _n_cores());

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    g_assert_cmpuint(res[i].cache_size, ==, STEP_SIZE * (i + 1));
    g_assert_cmpuint(res[i].n_req, ==, req_cnt_true);
    g_assert_cmpuint(res[i].n_miss, ==, miss_cnt_true[i]);
    g_assert_cmpuint(res[i].n_req_byte, ==, req_byte_true);
    g_assert_cmpuint(res[i].n_miss_byte, ==, miss_byte_true[i]);
  }
  g_free(res);

  cache->cache_free(cache);
}

static void test_simulator_with_ttl(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {93249, 87891, 83078, 81567,
                              72422, 72260, 72163, 72083};
  uint64_t miss_byte_true[] = {4037085184, 3838869504, 3666520064, 3614002688,
                               3084368896, 3082371584, 3078228480, 3075253760};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
                                     .default_ttl = 2400};
  cache_t *cache = LRU_init(cc_params, NULL);
  g_assert_true(cache != NULL);

  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());
  //  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
  //    printf("size %" PRIu64 " - req %" PRIu64 " - miss %" PRIu64
  //           " - req_bytes %" PRIu64 " - miss_bytes "
  //           "%" PRIu64 " - occupied_size %" PRIu64 " - stored_obj %" PRIu64
  //           " - expired_obj %" PRIu64 " - "
  //           "expired_bytes %" PRIu64 "\n",
  //           res[i].cache_size, res[i].n_req, res[i].n_miss,
  //           res[i].n_req_byte, res[i].n_miss_byte,
  //           res[i].cache_state.occupied_size, res[i].cache_state.n_obj,
  //           res[i].cache_state.expired_obj_cnt,
  //           res[i].cache_state.expired_bytes);
  //  }

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    g_assert_cmpuint(res[i].cache_size, ==, STEP_SIZE * (i + 1));
    g_assert_cmpuint(res[i].n_req, ==, req_cnt_true);
    g_assert_cmpuint(res[i].n_req_byte, ==, req_byte_true);
    g_assert_cmpuint(res[i].n_miss, ==, miss_cnt_true[i]);
    g_assert_cmpuint(res[i].n_miss_byte, ==, miss_byte_true[i]);
  }
  g_free(res);

  cache->cache_free(cache);
}

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t *reader;

  reader = setup_plaintxt_reader_num();
  g_test_add_data_func_full("/libCacheSim/simulator_no_size_plain_num", reader,
                            test_simulator_no_size, test_teardown);

  reader = setup_plaintxt_reader_str();
  g_test_add_data_func_full("/libCacheSim/simulator_no_size_plain_str", reader,
                            test_simulator_no_size, test_teardown);

  reader = setup_csv_reader_obj_num();
  g_test_add_data_func_full("/libCacheSim/simulator_csv_num", reader,
                            test_simulator, test_teardown);

  reader = setup_csv_reader_obj_str();
  g_test_add_data_func_full("/libCacheSim/simulator_csv_str", reader,
                            test_simulator, test_teardown);

  reader = setup_binary_reader();
  g_test_add_data_func_full("/libCacheSim/simulator_binary", reader,
                            test_simulator, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func_full("/libCacheSim/simulator_vscsi", reader,
                            test_simulator, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func_full("/libCacheSim/simulator_warmup1", reader,
                            test_simulator_with_warmup1, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func_full("/libCacheSim/simulator_warmup2", reader,
                            test_simulator_with_warmup2, test_teardown);

#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  reader = setup_vscsi_reader();
  g_test_add_data_func_full("/libCacheSim/simulator_with_ttl", reader,
                            test_simulator_with_ttl, test_teardown);
#endif

  return g_test_run();
}