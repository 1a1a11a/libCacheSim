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

static void test_AdaptSize(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {83204, 80907, 77835, 77086, 76173, 76158, 76158, 76158};
  uint64_t miss_byte_true[] = {3996894720, 3916923392, 3790021120, 3751927808, 3695680512, 3695609344, 3695609344, 3695609344};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("AdaptSize", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true, miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Size(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {93374, 89783, 83572, 81722, 72494, 72104, 71972, 71704};
  uint64_t miss_byte_true[] = {4214303232, 4061242368, 3778040320, 3660569600, 3100927488, 3078128640, 3075403776, 3061662720};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Size", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true, miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_SizeProb(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {93371, 89122, 83635, 81935, 73293, 72963, 72737, 71949};
  uint64_t miss_byte_true[] = {4214365696, 4030683648, 3781775872, 3671897088, 3151684096, 3133195264, 3123936256, 3078763520};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("SizeProb", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true, miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_BloomFilter(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {94816, 90386, 88417, 85744, 82344, 79504, 77058, 76979};
  uint64_t miss_byte_true[] = {4193502720, 3979631104, 3877562880, 3716727296, 3503820288, 3323299328, 3257762304, 3254848512};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("BloomFilter", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true, miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void empty_test(gconstpointer user_data) { ; }

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

  g_test_add_data_func("/libCacheSim/admissionAlgo_AdaptSize", reader, test_AdaptSize);
  g_test_add_data_func("/libCacheSim/admissionAlgo_Size", reader, test_Size);
  g_test_add_data_func("/libCacheSim/admissionAlgo_SizeProb", reader, test_SizeProb);
  g_test_add_data_func("/libCacheSim/admissionAlgo_BloomFilter", reader, test_BloomFilter);

  return g_test_run();
}
