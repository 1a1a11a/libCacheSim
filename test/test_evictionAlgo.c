//
// Created by Juncheng Yang on 11/21/19.
//

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
} cache_test_data_t;

// Test data definitions (ordered alphabetically by algorithm name)
static const cache_test_data_t test_data_truth[] = {
    {.cache_name = "ARC",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {90252, 85861, 78168, 74297, 67381, 65685, 64439, 64772},
     .miss_byte_true = {4068098560, 3821026816, 3525644800, 3296890368,
                        2868538880, 2771180032, 2699484672, 2712971264}},
    {.cache_name = "Belady",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {79256, 70724, 65481, 61594, 59645, 57599, 50873, 48974},
     .miss_byte_true = {3472532480, 2995165696, 2726689792, 2537648128,
                        2403427840, 2269212672, 2134992896, 2029769728}},
    {.cache_name = "BeladySize",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {74329, 64553, 60315, 56522, 54546, 52618, 50580, 48974},
     .miss_byte_true = {3510350848, 3046487552, 2774967808, 2537689600,
                        2403425280, 2269210112, 2135005184, 2029769728}},
    {.cache_name = "Cacheus",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {90052, 82866, 77130, 77115, 69828, 68435, 67930, 66993},
     .miss_byte_true = {4068200448, 3757362176, 3439912448, 3359079424,
                        3018722816, 2928907776, 2867576832, 2834809856}},
    {.cache_name = "CAR",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {90522, 83605, 78063, 75772, 67384, 65687, 64439, 64376},
     .miss_byte_true = {4084188160, 3769425920, 3525660160, 3394717696,
                        2868551168, 2771188224, 2699423232, 2696345600}},
    {.cache_name = "Clock",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {93313, 89775, 83411, 81328, 74815, 72283, 71927, 64456},
     .miss_byte_true = {4213887488, 4064512000, 3762650624, 3644467200,
                        3256760832, 3091688448, 3074241024, 2697378816}},
    {.cache_name = "ClockPro",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {96390, 92614, 88911, 85894, 82276, 73203, 63728, 57544},
     .miss_byte_true = {4163599360, 3922361856, 3700721152, 3491452416,
                        3245322240, 2653708288, 2413087744, 2293678592}},
    {.cache_name = "CR_LFU",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {92095, 88257, 84839, 81885, 78348, 69281, 61350, 54894},
     .miss_byte_true = {4141293056, 3900042240, 3686207488, 3481216000,
                        3238197760, 2646171648, 2408963072, 2289538048}},
    {.cache_name = "FIFO",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {93403, 89386, 84387, 84025, 72498, 72228, 72182, 72140},
     .miss_byte_true = {4213112832, 4052646400, 3829170176, 3807412736,
                        3093146112, 3079525888, 3079210496, 3077547520}},
    {.cache_name = "GDSF",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {89070, 84750, 74850, 70490, 67923, 64180, 61027, 58721},
     .miss_byte_true = {4210726912, 4057058816, 3719176192, 3436855296,
                        3271648256, 3029728768, 2828456448, 2677800448}},
    {.cache_name = "Hyperbolic",
     .hashpower = 18,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {92924, 89470, 83452, 81234, 74544, 71234, 69356, 65338},
     .miss_byte_true = {4213586432, 4064826368, 3766646272, 3644941824,
                        3245021184, 3035783168, 2939981312, 2754100224}},
    {.cache_name = "LeCaR",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {93374, 89067, 80230, 81526, 72159, 67712, 65206, 64541},
     .miss_byte_true = {4214303232, 4021100032, 3593971712, 3652036096,
                        3075125760, 2886052864, 2735856128, 2698478080}},
    {.cache_name = "LFU",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {91699, 86720, 78578, 76707, 69945, 66221, 64445, 64376},
     .miss_byte_true = {4158632960, 3917211648, 3536227840, 3455379968,
                        3035580416, 2801699328, 2699456000, 2696345600}},
    {.cache_name = "LFUDA",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {92637, 88601, 82001, 80240, 73214, 71386, 70415, 71128},
     .miss_byte_true = {4200012288, 3993467904, 3673375232, 3579174400,
                        3164476928, 3046658048, 2998682624, 3027994112}},
    {.cache_name = "LHD",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {90534, 86891, 82334, 77339, 71355, 66938, 63677, 61116},
     .miss_byte_true = {4211037696, 4059153920, 3834546176, 3596945408,
                        3326034944, 3115964416, 2951718912, 2804600832}},
    {.cache_name = "LIRS",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {89819, 79237, 73143, 70363, 68405, 64494, 58640, 53924},
     .miss_byte_true = {4060558336, 3525952512, 3199406080, 3011810816,
                        2848310272, 2580918784, 2361375744, 2288325120}},
    {.cache_name = "LRU",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {93374, 89783, 83572, 81722, 72494, 72104, 71972, 71704},
     .miss_byte_true = {4214303232, 4061242368, 3778040320, 3660569600,
                        3100927488, 3078128640, 3075403776, 3061662720}},
    {.cache_name = "MRU",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {100738, 95058, 89580, 85544, 81725, 77038, 71070, 66919},
     .miss_byte_true = {4105477120, 3784799744, 3493475840, 3280475648,
                        3069635072, 2856241152, 2673937408, 2539762688}},
    {.cache_name = "QDLP-FIFO",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {88746, 80630, 76450, 71638, 67380, 65680, 66125, 64417},
     .miss_byte_true = {4008265728, 3625704960, 3330610176, 3099731456,
                        2868538880, 2771098112, 2734977024, 2697751552}},
    {.cache_name = "Random",
     .hashpower = 12,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {92457, 88582, 84459, 80277, 76132, 72134, 68230, 64225},
     .miss_byte_true = {4170166272, 3975292416, 3757524992, 3539850752,
                        3321110016, 3113551360, 2917275648, 2725705216}},
    {.cache_name = "S3-FIFO",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {90117, 80915, 75060, 72191, 69815, 65542, 60799, 56045},
     .miss_byte_true = {4058576896, 3573827584, 3244417024, 3061737984,
                        2898109952, 2628363776, 2425027072, 2327934464}},
    {.cache_name = "S3-FIFOv0",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {89307, 82387, 77041, 76791, 71300, 70343, 70455, 70355},
     .miss_byte_true = {4040718336, 3703628800, 3353047552, 3282235904,
                        3038256128, 2980646912, 2984458752, 2979649536}},
    {.cache_name = "Sieve",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {91699, 86720, 78578, 76707, 69945, 66221, 64445, 64376},
     .miss_byte_true = {4158632960, 3917211648, 3536227840, 3455379968,
                        3035580416, 2801699328, 2699456000, 2696345600}},
    {.cache_name = "SLRU",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {89624, 86725, 82781, 80203, 75388, 65645, 59035, 56063},
     .miss_byte_true = {4123085312, 3915534848, 3690704896, 3493027840,
                        3174708736, 2661464064, 2507604992, 2439981056}},
    {.cache_name = "SR_LRU",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {90043, 83978, 81482, 77727, 72611, 72059, 67836, 65739},
     .miss_byte_true = {4068758016, 3792818176, 3639756288, 3379609600,
                        3165339648, 3058814976, 2862775296, 2774183936}},
#if defined(ENABLE_3L_CACHE) && ENABLE_3L_CACHE == 1
    {.cache_name = "3LCache",
     .hashpower = 20,
     .req_cnt_true = 113872,
     .req_byte_true = 4368040448,
     .miss_cnt_true = {93374, 89783, 83572, 81722, 72494, 72104, 71972, 71704},
     .miss_byte_true = {4214303232, 4061242368, 3778040320, 3660569600,
                        3100927488, 3078128640, 3075403776, 3061662720}},
#endif /* ENABLE_3L_CACHE */
};

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

  printf("uint64_t miss_cnt[] = {");
  printf("%ld", (long)res[0].n_miss);
  for (uint64_t i = 1; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf(", %ld", (long)res[i].n_miss);
  }
  printf("};\n");

  printf("uint64_t miss_byte[] = {");
  printf("%ld", (long)res[0].n_miss_byte);
  for (uint64_t i = 1; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf(", %ld", (long)res[i].n_miss_byte);
  }
  printf("};\n");
}

// Generic test function that works for all cache algorithms
static void test_cache_algorithm(gconstpointer user_data,
                                 const cache_test_data_t *test_data) {
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
static void test_ARC(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[0]);
}

static void test_Belady(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[1]);
}

static void test_BeladySize(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[2]);
}

static void test_Cacheus(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[3]);
}

static void test_CAR(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[4]);
}

static void test_Clock(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[5]);
}

static void test_ClockPro(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[6]);
}

static void test_CR_LFU(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[7]);
}

static void test_FIFO(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[8]);
}

static void test_GDSF(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[9]);
}

static void test_Hyperbolic(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[10]);
}

static void test_LeCaR(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[11]);
}

static void test_LFU(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[12]);
}

static void test_LFUCpp(gconstpointer user_data) {
  // LFUCpp uses the same test data as LFU
  test_cache_algorithm(user_data, &test_data_truth[12]);
}

static void test_LFUDA(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[13]);
}

static void test_LHD(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[14]);
}

static void test_LIRS(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[15]);
}

static void test_LRU(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[16]);
}

static void test_MRU(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[17]);
}

static void test_QDLP_FIFO(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[18]);
}

static void test_Random(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[19]);
}

static void test_S3FIFO(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[20]);
}

static void test_S3FIFOv0(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[21]);
}

static void test_Sieve(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[22]);
}

static void test_SLRU(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[23]);
}

static void test_SR_LRU(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[24]);
}

#if defined(ENABLE_3L_CACHE) && ENABLE_3L_CACHE == 1
static void test_3LCache(gconstpointer user_data) {
  test_cache_algorithm(user_data, &test_data_truth[25]);
}
#endif /* ENABLE_3L_CACHE */

static void test_WTinyLFU(gconstpointer user_data) {
  // TODO: to be implemented
}

static void empty_test(gconstpointer user_data) { ; }

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  srand(0);           // for reproducibility
  set_rand_seed(42);  // Use fixed seed for cross-platform consistency

  reader_t *reader;

  // do not use these two because object size change over time and
  // not all algorithms can handle the object size change correctly
  // reader = setup_csv_reader_obj_num();
  // reader = setup_vscsi_reader();

  reader = setup_oracleGeneralBin_reader();

  // Test registrations (ordered alphabetically)
  g_test_add_data_func("/libCacheSim/cacheAlgo_ARC", reader, test_ARC);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Cacheus", reader, test_Cacheus);
  g_test_add_data_func("/libCacheSim/cacheAlgo_CAR", reader, test_CAR);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Clock", reader, test_Clock);
  g_test_add_data_func("/libCacheSim/cacheAlgo_ClockPro", reader,
                       test_ClockPro);
  g_test_add_data_func("/libCacheSim/cacheAlgo_CR_LFU", reader, test_CR_LFU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_FIFO", reader, test_FIFO);
  g_test_add_data_func("/libCacheSim/cacheAlgo_GDSF", reader, test_GDSF);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Hyperbolic", reader,
                       test_Hyperbolic);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LeCaR", reader, test_LeCaR);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LFU", reader, test_LFU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LFUCpp", reader, test_LFUCpp);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LFUDA", reader, test_LFUDA);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LHD", reader, test_LHD);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LIRS", reader, test_LIRS);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LRU", reader, test_LRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_MRU", reader, test_MRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_QDLP_FIFO", reader,
                       test_QDLP_FIFO);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Random", reader, test_Random);
  g_test_add_data_func("/libCacheSim/cacheAlgo_S3FIFO", reader, test_S3FIFO);
  g_test_add_data_func("/libCacheSim/cacheAlgo_S3FIFOv0", reader,
                       test_S3FIFOv0);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Sieve", reader, test_Sieve);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SLRU", reader, test_SLRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SR_LRU", reader, test_SR_LRU);

#if defined(ENABLE_3L_CACHE) && ENABLE_3L_CACHE == 1
  g_test_add_data_func("/libCacheSim/cacheAlgo_3LCache", reader, test_3LCache);
#endif /* ENABLE_3L_CACHE */

  // Belady algorithms require reader that has next access information
  // and can only use oracleGeneral trace (which we're using)
  g_test_add_data_func("/libCacheSim/cacheAlgo_Belady", reader, test_Belady);
  g_test_add_data_func("/libCacheSim/cacheAlgo_BeladySize", reader,
                       test_BeladySize);

  g_test_add_data_func_full("/libCacheSim/empty", reader, empty_test,
                            test_teardown);

  return g_test_run();
}
