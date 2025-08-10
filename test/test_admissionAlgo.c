#include "common.h"

// Constants
static const uint64_t g_req_cnt_true = 113872;
static const uint64_t g_req_byte_true = 4368040448;
static const uint64_t NUM_TEST_SIZES = 8;

// Test data structure
typedef struct {
  const char *cache_name;
  uint64_t hashpower;
  uint64_t req_cnt_true;
  uint64_t req_byte_true;
  uint64_t miss_cnt_true[8];
  uint64_t miss_byte_true[8];
} admission_test_data_t;

// Test data definitions (ordered alphabetically by algorithm name)
static const admission_test_data_t test_data_truth[] = {
    {.cache_name = "AdaptSize",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {83204, 80907, 77835, 77086, 76173, 76158, 76158, 76158},
     .miss_byte_true = {3996894720, 3916923392, 3790021120, 3751927808,
                        3695680512, 3695609344, 3695609344, 3695609344}},
    {.cache_name = "BloomFilter",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {94816, 90386, 88417, 85744, 82344, 79504, 77058, 76979},
     .miss_byte_true = {4193502720, 3979631104, 3877562880, 3716727296,
                        3503820288, 3323299328, 3257762304, 3254848512}},
    {.cache_name = "Size",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {93374, 89783, 83572, 81722, 72494, 72104, 71972, 71704},
     .miss_byte_true = {4214303232, 4061242368, 3778040320, 3660569600,
                        3100927488, 3078128640, 3075403776, 3061662720}},
    {.cache_name = "SizeProb",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {93371, 89122, 83635, 81935, 73293, 72963, 72737, 71949},
     .miss_byte_true = {4214365696, 4030683648, 3781775872, 3671897088,
                        3151684096, 3133195264, 3123936256, 3078763520}}};

static void _verify_profiler_results(const cache_stat_t *res,
                                     uint64_t num_of_sizes,
                                     uint64_t req_cnt_true,
                                     const uint64_t *miss_cnt_true,
                                     uint64_t req_byte_true,
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

// Generic test function that works for all admission algorithms
static void test_admission_algorithm(gconstpointer user_data,
                                     const admission_test_data_t *test_data) {
  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
                                     .hashpower = test_data->hashpower,
                                     .default_ttl = DEFAULT_TTL};

  cache_t *cache =
      create_test_cache(test_data->cache_name, cc_params, reader, NULL);
  g_assert_true(cache != NULL);

  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores(), false);

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, test_data->req_cnt_true,
                           test_data->miss_cnt_true, test_data->req_byte_true,
                           test_data->miss_byte_true);

  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t) * (CACHE_SIZE / STEP_SIZE), res);
}

// Individual test functions (ordered alphabetically)
static void test_AdaptSize(gconstpointer user_data) {
  test_admission_algorithm(user_data, &test_data_truth[0]);
}

static void test_BloomFilter(gconstpointer user_data) {
  test_admission_algorithm(user_data, &test_data_truth[1]);
}

static void test_Size(gconstpointer user_data) {
  test_admission_algorithm(user_data, &test_data_truth[2]);
}

static void test_SizeProb(gconstpointer user_data) {
  test_admission_algorithm(user_data, &test_data_truth[3]);
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

  // Test registrations (ordered alphabetically)
  g_test_add_data_func("/libCacheSim/admissionAlgo_AdaptSize", reader,
                       test_AdaptSize);
  g_test_add_data_func("/libCacheSim/admissionAlgo_BloomFilter", reader,
                       test_BloomFilter);
  g_test_add_data_func("/libCacheSim/admissionAlgo_Size", reader, test_Size);
  g_test_add_data_func_full("/libCacheSim/admissionAlgo_SizeProb", reader,
                            test_SizeProb, test_teardown);

  return g_test_run();
}
