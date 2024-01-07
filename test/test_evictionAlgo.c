//
// Created by Juncheng Yang on 11/21/19.
//

#include "../libCacheSim/utils/include/mymath.h"
#include "common.h"

static const uint64_t g_req_cnt_true = 113872, g_req_byte_true = 4368040448;

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

static void test_LRU(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {93374, 89783, 83572, 81722,
                              72494, 72104, 71972, 71704};
  uint64_t miss_byte_true[] = {4214303232, 4061242368, 3778040320, 3660569600,
                               3100927488, 3078128640, 3075403776, 3061662720};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Clock(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {93313, 89775, 83411, 81328,
                              74815, 72283, 71927, 64456};
  uint64_t miss_byte_true[] = {4213887488, 4064512000, 3762650624, 3644467200,
                               3256760832, 3091688448, 3074241024, 2697378816};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Clock", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_FIFO(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {93403, 89386, 84387, 84025,
                              72498, 72228, 72182, 72140};
  uint64_t miss_byte_true[] = {4213112832, 4052646400, 3829170176, 3807412736,
                               3093146112, 3079525888, 3079210496, 3077547520};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("FIFO", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Belady(gconstpointer user_data) {
  /* the request byte is different from others because the oracleGeneral
   * trace removes all object size changes (and use the size of last appearance
   * of an object as the object size throughout the trace */
  uint64_t req_cnt_true = 113872, req_byte_true = 4368040448;
  uint64_t miss_cnt_true[] = {79256, 70724, 65481, 61594,
                              59645, 57599, 50873, 48974};
  uint64_t miss_byte_true[] = {3472532480, 2995165696, 2726689792, 2537648128,
                               2403427840, 2269212672, 2134992896, 2029769728};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Belady", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_BeladySize(gconstpointer user_data) {
  /* the request byte is different from others because the oracleGeneral
   * trace removes all object size changes (and use the size of last appearance
   * of an object as the object size throughout the trace */
  uint64_t req_cnt_true = 113872, req_byte_true = 4368040448;
  uint64_t miss_cnt_true[] = {74329, 64524, 60279, 56514,
                              54539, 52613, 50581, 48974};
  uint64_t miss_byte_true[] = {3507168256, 3044453888, 2773635072, 2537643008,
                               2403463680, 2269248512, 2135011840, 2029769728};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("BeladySize", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Random(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {92444, 88637, 84247, 79875,
                              75206, 71042, 67096, 63182};
  uint64_t miss_byte_true[] = {4165255168, 3976227840, 3744436736, 3506359808,
                               3300872704, 3055019008, 2837280256, 2642700288};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 12, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Random", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LFU(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {91699, 86720, 78578, 76707,
                              69945, 66221, 64445, 64376};
  uint64_t miss_byte_true[] = {4158632960, 3917211648, 3536227840, 3455379968,
                               3035580416, 2801699328, 2699456000, 2696345600};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LFU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_LFUCpp(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {91699, 86720, 78578, 76707,
                              69945, 66221, 64445, 64376};
  uint64_t miss_byte_true[] = {4158632960, 3917211648, 3536227840, 3455379968,
                               3035580416, 2801699328, 2699456000, 2696345600};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LFU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_GDSF(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {89068, 84750, 74853, 70488,
                              67919, 64180, 61029, 58721};
  uint64_t miss_byte_true[] = {4210604032, 4057062912, 3719372800, 3436728320,
                               3271431168, 3029728768, 2828587520, 2677800448};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("GDSF", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LHD(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {90500, 86928, 82211, 77256,
                              71259, 67001, 63734, 61237};
  uint64_t miss_byte_true[] = {4209021952, 4060000768, 3824893440, 3594843136,
                               3319633408, 3119866880, 2953787392, 2810359808};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LHD", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Hyperbolic(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {92836, 89281, 84407, 80420,
                              76299, 71843, 68884, 65654};
  uint64_t miss_byte_true[] = {4210119680, 4050535424, 3806349824, 3578494976,
                               3352331264, 3092705792, 2910994944, 2769552384};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 18, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Hyperbolic", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LeCaR(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {93269, 89035, 80244, 80815,
                              72165, 67720, 65487, 66535};
  uint64_t miss_byte_true[] = {4214232576, 4018871808, 3595043328, 3604504064,
                               3075584512, 2886519808, 2748467712, 2755397120};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LeCaR", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Cacheus(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {89419, 82885, 80096, 73107,
                              69773, 68192, 67629, 66960};
  uint64_t miss_byte_true[] = {4036696064, 3757154816, 3554868736, 3182398976,
                               3020529664, 2912187904, 2865267712, 2835341312};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Cacheus", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_SR_LRU(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {90043, 83978, 81481, 77724,
                              72611, 72058, 67837, 65739};
  uint64_t miss_byte_true[] = {4068758016, 3792818176, 3639694848, 3379471872,
                               3165339648, 3058749440, 2862783488, 2774183936};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("SR_LRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_CR_LFU(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {92095, 88257, 84839, 81885,
                              78348, 69281, 61350, 54894};
  uint64_t miss_byte_true[] = {4141293056, 3900042240, 3686207488, 3481216000,
                               3238197760, 2646171648, 2408963072, 2289538048};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("CR_LFU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LFUDA(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {92637, 88601, 82001, 80240,
                              73214, 71386, 70415, 71128};
  uint64_t miss_byte_true[] = {4200012288, 3993467904, 3673375232, 3579174400,
                               3164476928, 3046658048, 2998682624, 3027994112};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LFUDA", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_MRU(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {100738, 95058, 89580, 85544,
                              81725,  77038, 71070, 66919};
  uint64_t miss_byte_true[] = {4105477120, 3784799744, 3493475840, 3280475648,
                               3069635072, 2856241152, 2673937408, 2539762688};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("MRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_ARC(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {90252, 85861, 78168, 74297,
                              67381, 65685, 64439, 64772};
  uint64_t miss_byte_true[] = {4068098560, 3821026816, 3525644800, 3296890368,
                               2868538880, 2771180032, 2699484672, 2712971264};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("ARC", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_SLRU(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {89624, 86725, 82781, 80203,
                              75388, 65645, 59035, 56063};
  uint64_t miss_byte_true[] = {4123085312, 3915534848, 3690704896, 3493027840,
                               3174708736, 2661464064, 2507604992, 2439981056};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("SLRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_QDLP_FIFO(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {88746, 80630, 76450, 71638,
                              67380, 65680, 66125, 64417};
  uint64_t miss_byte_true[] = {4008265728, 3625704960, 3330610176, 3099731456,
                               2868538880, 2771098112, 2734977024, 2697751552};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("QDLP-FIFO", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_S3FIFO(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {89307, 82387, 77041, 76791,
                              71300, 70343, 70455, 70355};
  uint64_t miss_byte_true[] = {4040718336, 3703628800, 3353047552, 3282235904,
                               3038256128, 2980646912, 2984458752, 2979649536};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("S3-FIFO", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Sieve(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {91699, 86720, 78578, 76707,
                              69945, 66221, 64445, 64376};
  uint64_t miss_byte_true[] = {4158632960, 3917211648, 3536227840, 3455379968,
                               3035580416, 2801699328, 2699456000, 2696345600};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Sieve", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_WTinyLFU(gconstpointer user_data) {
  // TODO: to be implemented
}

static void test_LIRS(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {89819, 79237, 73143, 70363,
                              68405, 64494, 58640, 53924};
  uint64_t miss_byte_true[] = {4060558336, 3525952512, 3199406080, 3011810816,
                               2848310272, 2580918784, 2361375744, 2288325120};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LIRS", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, g_req_cnt_true,
                           miss_cnt_true, g_req_byte_true, miss_byte_true);
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
  // reader = setup_vscsi_reader_with_ignored_obj_size();
  g_test_add_data_func("/libCacheSim/cacheAlgo_Sieve", reader, test_Sieve);
  g_test_add_data_func("/libCacheSim/cacheAlgo_S3FIFO", reader, test_S3FIFO);
  g_test_add_data_func("/libCacheSim/cacheAlgo_QDLP_FIFO", reader,
                       test_QDLP_FIFO);

  g_test_add_data_func("/libCacheSim/cacheAlgo_LRU", reader, test_LRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SLRU", reader, test_SLRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_ARC", reader, test_ARC);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LeCaR", reader, test_LeCaR);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SR_LRU", reader, test_SR_LRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_CR_LFU", reader, test_CR_LFU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Cacheus", reader, test_Cacheus);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Hyperbolic", reader,
                       test_Hyperbolic);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LIRS", reader, test_LIRS);

  g_test_add_data_func("/libCacheSim/cacheAlgo_Clock", reader, test_Clock);
  g_test_add_data_func("/libCacheSim/cacheAlgo_FIFO", reader, test_FIFO);
  g_test_add_data_func("/libCacheSim/cacheAlgo_MRU", reader, test_MRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Random", reader, test_Random);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LFU", reader, test_LFU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LFUDA", reader, test_LFUDA);

  g_test_add_data_func("/libCacheSim/cacheAlgo_LFUCpp", reader, test_LFUCpp);
  g_test_add_data_func("/libCacheSim/cacheAlgo_GDSF", reader, test_GDSF);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LHD", reader, test_LHD);

  /* Belady requires reader that has next access information and can only use
   * oracleGeneral trace */
  g_test_add_data_func("/libCacheSim/cacheAlgo_Belady", reader, test_Belady);
  g_test_add_data_func("/libCacheSim/cacheAlgo_BeladySize", reader,
                       test_BeladySize);

  g_test_add_data_func_full("/libCacheSim/empty", reader, empty_test,
                            test_teardown);

  return g_test_run();
}
