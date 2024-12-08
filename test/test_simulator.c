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
  uint64_t miss_cnt_true[] = {99411, 96397, 95652, 95370, 95182, 94997, 94891, 94816};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = cache_size, .default_ttl = 0};
  cache_t *cache = LRU_init(cc_params, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, step_size, NULL, 0, 0, _n_cores(), false);

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
  // result when LRU considers object size change over time
  // uint64_t miss_cnt_true[] = {93161, 87794, 82945, 81433,
  //                             72250, 72083, 71969, 71716};
  // uint64_t miss_byte_true[] = {4036642816, 3838227968, 3664065536,
  // 3611544064,
  //                              3081624576, 3079594496, 3075357184,
  //                              3060711936};

  // result when LRU DOES NOT considers object size change over time
  uint64_t miss_cnt_true[] = {93151, 87793, 83135, 81609, 72481, 72106, 71973, 71702};
  uint64_t miss_byte_true[] = {4035348480, 3841399808, 3660518400, 3613104640,
                               3087721984, 3080147456, 3075377664, 3059534336};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .default_ttl = 0, .hashpower = 16, .consider_obj_metadata = false};
  cache_t *cache = LRU_init(cc_params, NULL);
  g_assert_true(cache != NULL);

  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores(), false);
  // for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
  //   printf(
  //       "cache size: %lu, n_req: %lu, n_req_byte: %lu, n_miss: %8lu %16lu\n
  //       ", res[i].cache_size, res[i].n_req, res[i].n_req_byte, res[i].n_miss,
  //       res[i].n_miss_byte);
  // }

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    g_assert_cmpuint(res[i].cache_size, ==, STEP_SIZE * (i + 1));
    g_assert_cmpuint(res[i].n_req, ==, req_cnt_true);
    g_assert_cmpuint(res[i].n_req_byte, ==, req_byte_true);
    g_assert_cmpuint(res[i].n_miss, ==, miss_cnt_true[i]);
    g_assert_cmpuint(res[i].n_miss_byte, ==, miss_byte_true[i]);
  }
  g_free(res);

  uint64_t cache_sizes[] = {STEP_SIZE, STEP_SIZE * 2, STEP_SIZE * 4, STEP_SIZE * 7};
  res = simulate_at_multi_sizes(reader, cache, 4, cache_sizes, NULL, 0, 0, _n_cores(), false);
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

  res = simulate_with_multi_caches(reader, caches, 4, NULL, 0, 0, _n_cores(), false, false);
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
  uint64_t miss_cnt_true[] = {92999, 87632, 82972, 81443, 72316, 71934, 71766, 71307};
  uint64_t miss_byte_true[] = {4033582080, 3839580160, 3658690560, 3611252224,
                               3085914624, 3078132736, 3071579648, 3043186176};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = LRU_init(cc_params, NULL);
  g_assert_true(cache != NULL);

  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, STEP_SIZE, reader, 0, 0, _n_cores(), false);

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    // printf("cache size: %lu, n_req: %lu, n_req_byte: %lu, n_miss: %8lu
    // %16lu\n",
    //        res[i].cache_size, res[i].n_req, res[i].n_req_byte, res[i].n_miss,
    //        res[i].n_miss_byte);
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
  uint64_t miss_cnt_true[] = {75018, 69709, 65274, 63750, 57484, 57124, 56991, 56720};
  uint64_t miss_byte_true[] = {3035036672, 2842572288, 2672791552, 2625385984,
                               2269361664, 2261869056, 2257099264, 2241255936};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = LRU_init(cc_params, NULL);
  g_assert_true(cache != NULL);

  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, STEP_SIZE, NULL, 0.2, 0, _n_cores(), false);

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    // printf("cache size: %lu, n_req: %lu, n_req_byte: %lu, n_miss: %8lu
    // %16lu\n",
    //        res[i].cache_size, res[i].n_req, res[i].n_req_byte, res[i].n_miss,
    //        res[i].n_miss_byte);
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
  uint64_t miss_cnt_true[] = {93240, 87890, 83268, 81743, 72649, 72284, 72165, 72086};
  uint64_t miss_byte_true[] = {4035856384, 3842041344, 3662972928, 3615563264,
                               3090454016, 3082932736, 3078236672, 3075259904};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 2400};
  cache_t *cache = LRU_init(cc_params, NULL);
  g_assert_true(cache != NULL);

  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf("cache size: %lu, n_req: %ld, n_req_byte: %ld, n_miss: %8ld %16ld\n", (unsigned long)res[i].cache_size,
           (long)res[i].n_req, (long)res[i].n_req_byte, (long)res[i].n_miss, (long)res[i].n_miss_byte);
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
  g_test_add_data_func_full("/libCacheSim/simulator_no_size_plain_num", reader, test_simulator_no_size, test_teardown);

  reader = setup_plaintxt_reader_str();
  g_test_add_data_func_full("/libCacheSim/simulator_no_size_plain_str", reader, test_simulator_no_size, test_teardown);

  reader = setup_csv_reader_obj_num();
  g_test_add_data_func_full("/libCacheSim/simulator_csv_num", reader, test_simulator, test_teardown);

  reader = setup_csv_reader_obj_str();
  g_test_add_data_func_full("/libCacheSim/simulator_csv_str", reader, test_simulator, test_teardown);

  reader = setup_binary_reader();
  g_test_add_data_func_full("/libCacheSim/simulator_binary", reader, test_simulator, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func_full("/libCacheSim/simulator_vscsi", reader, test_simulator, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func_full("/libCacheSim/simulator_warmup1", reader, test_simulator_with_warmup1, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func_full("/libCacheSim/simulator_warmup2", reader, test_simulator_with_warmup2, test_teardown);

#ifdef SUPPORT_TTL
  reader = setup_vscsi_reader();
  g_test_add_data_func_full("/libCacheSim/simulator_with_ttl", reader, test_simulator_with_ttl, test_teardown);
#endif

  return g_test_run();
}