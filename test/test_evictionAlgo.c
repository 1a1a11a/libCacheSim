//
// Created by Juncheng Yang on 11/21/19.
//

#include "../libCacheSim/utils/include/mymath.h"
#include "common.h"

// static const uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
static const uint64_t req_cnt_true = 113872, req_byte_true = 4368040448;

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
  printf("%ld", res[0].cache_size);
  for (uint64_t i = 1; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf(", %ld", res[i].cache_size);
  }
  printf("};\n");

  printf("uint64_t miss_cnt_true[] = {");
  printf("%ld", res[0].n_miss);
  for (uint64_t i = 1; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf(", %ld", res[i].n_miss);
  }
  printf("};\n");

  printf("uint64_t miss_byte_true[] = {");
  printf("%ld", res[0].n_miss_byte);
  for (uint64_t i = 1; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf(", %ld", res[i].n_miss_byte);
  }
  printf("};\n");

  // for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
  //   printf("%s cache size %16" PRIu64 " req %" PRIu64 " miss %8" PRIu64
  //          " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
  //          cache->cache_name, res[i].cache_size, res[i].n_req, res[i].n_miss,
  //          res[i].n_req_byte, res[i].n_miss_byte);
  // }
}

static void test_LRU(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {93161, 87794, 82945, 81433,
  //                             72250, 72083, 71969, 71716};
  // uint64_t miss_byte_true[] = {4036642816, 3838227968, 3664065536,
  // 3611544064,
  //                              3081624576, 3079594496, 3075357184,
  //                              3060711936};
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Clock(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {91385, 84061, 77353, 76506,
  //                             68994, 66441, 64819, 64376};
  // uint64_t miss_byte_true[] = {3990213632, 3692986368, 3434442752,
  // 3413374464,
  //                              2963407872, 2804032512, 2717934080,
  //                              2690728448};
  uint64_t miss_cnt_true[] = {91699, 86720, 78578, 76707,
                              69945, 66221, 64445, 64376};
  uint64_t miss_byte_true[] = {4158632960, 3917211648, 3536227840, 3455379968,
                               3035580416, 2801699328, 2699456000, 2696345600};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Clock", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_FIFO(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {93193, 87172, 84321, 83888,
  //                             72331, 72230, 72181, 72141};
  // uint64_t miss_byte_true[] = {4035451392, 3815613440, 3724681728,
  // 3751948288,
  //                              3083697664, 3081942528, 3081872384,
  //                              3080036864};
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_SFIFO_Merge(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {91243, 87077, 82766, 78182,
  //                             71638, 69212, 66147, 62359};
  // uint64_t miss_byte_true[] = {3996508672, 3823320576, 3620353024,
  // 3395847680,
  //                              3108824576, 2997791232, 2782127616,
  //                              2656834560};
  uint64_t miss_cnt_true[] = {91975, 88642, 84144, 79519,
                              72623, 69983, 64976, 62609};
  uint64_t miss_byte_true[] = {4207505920, 4025959936, 3816864768, 3548698112,
                               3165346304, 3058058240, 2802594816, 2676355072};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("SFIFO_Merge", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_SFIFO_Reinsertion(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {90854, 85386, 79839, 75702,
  //                             72785, 68092, 65945, 65403};
  // uint64_t miss_byte_true[] = {3982227968, 3767993344, 3531636224,
  // 3343059968,
  //                              3195103232, 2915291136, 2796444160,
  //                              2761173504};
  uint64_t miss_cnt_true[] = {91528, 86938, 81367, 77013,
                              73320, 68429, 65688, 65186};
  uint64_t miss_byte_true[] = {4204530176, 4008255488, 3725007872, 3475784192,
                               3264422912, 2956263936, 2775667200, 2756136448};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache =
      create_test_cache("SFIFO_Reinsertion", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LFU(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {91385, 84061, 77353, 76506,
  //                             68994, 66441, 64819, 64376};
  // uint64_t miss_byte_true[] = {3990213632, 3692986368, 3434442752,
  // 3413374464,
  //                              2963407872, 2804032512, 2717934080,
  //                              2690728448};
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_LFUCpp(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {91385, 84061, 77353, 76506,
  //                             68994, 66441, 64819, 64376};
  // uint64_t miss_byte_true[] = {3990213632, 3692986368, 3434442752,
  // 3413374464,
  //                              2963407872, 2804032512, 2717934080,
  //                              2690728448};
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LHD(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {88907, 84394, 80846, 76482,
  //                             70960, 66571, 63627, 61156};
  // uint64_t miss_byte_true[] = {3931543552, 3755170304, 3620246528,
  // 3455476736,
  //                              3240826880, 3055610368, 2918132736,
  //                              2789520896};
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Hyperbolic(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {92693, 87661, 82511, 80761,
  //                             74171, 71339, 69257, 65394};
  // uint64_t miss_byte_true[] = {4034421248, 3844452352, 3651789824,
  // 3582822400,
  //                              3211801600, 3040467456, 2936043520,
  //                              2761336320};
  uint64_t miss_cnt_true[] = {92904, 89464, 83622, 81002,
                              74508, 71437, 69556, 65220};
  uint64_t miss_byte_true[] = {4212533248, 4064380928, 3768431104, 3630879744,
                               3243378176, 3050633728, 2942699008, 2752167424};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 18, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Hyperbolic", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LeCaR(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {93175, 85561, 81814, 81275,
  //                             72267, 69446, 71287, 68551};
  // uint64_t miss_byte_true[] = {
  //     4038078976, 3744889344, 3612951040, 3607336960,
  //     3072626688, 2939962368, 3031271936, 2857521152,
  // };
  uint64_t miss_cnt_true[] = {93396, 89787, 83637, 80677,
                              72165, 68895, 67242, 68174};
  uint64_t miss_byte_true[] = {4215944192, 4062137344, 3776311808, 3596392448,
                               3073327616, 2894902784, 2795203584, 2835615744};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LeCaR", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Cacheus(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {89628, 82684, 80034, 72740,
  //                             69192, 67763, 67477, 67048};
  // uint64_t miss_byte_true[] = {3902360064, 3654431232, 3440451072,
  // 3151679488,
  //                              2955547648, 2879830016, 2864226304,
  //                              2836520448};
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_SR_LRU(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {89493, 83770, 81321, 76521,
  //                             72708, 71775, 67375, 65728};
  // uint64_t miss_byte_true[] = {3925441024, 3711699456, 3518652928,
  // 3296385024,
  //                              3152901120, 3038793216, 2867397120,
  //                              2769311232};
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_CR_LFU(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {92084, 88226, 84701, 80430,
  //                             75909, 66138, 59767, 54763};
  // uint64_t miss_byte_true[] = {3972743680, 3726964736, 3516974592,
  // 3308064768,
  //                              3078354432, 2621426176, 2372660736,
  //                              2297313280};
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LFUDA(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {92100, 88062, 81473, 79459,
                              73869, 69448, 68984, 66185};
  uint64_t miss_byte_true[] = {4174408192, 3966416896, 3641486336, 3540448256,
                               3213380608, 2955176960, 2944822784, 2810384896};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LFUDA", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_MRU(gconstpointer user_data) {
  // uint64_t miss_cnt_true[] = {100096, 93466, 87001, 82124,
  //                             77443,  71956, 68252, 64936};
  // uint64_t miss_byte_true[] = {3929311744, 3599883264, 3304649728,
  // 3097044480,
  //                              2896690176, 2697243648, 2576163840,
  //                              2439055360};
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
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_ARC(gconstpointer user_data) {
  int64_t miss_cnt_true[] = {90291, 86747, 78133, 74297,
                             67381, 65685, 64443, 64775};
  uint64_t miss_byte_true[] = {4070830592, 3842613248, 3525521408, 3296890368,
                               2868538880, 2771180032, 2699746816, 2713581056};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("ARC", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_SLRU(gconstpointer user_data) {
  uint64_t miss_cnt_true[] = {91204, 87638, 84193, 80995,
                              77838, 70264, 61030, 56838};
  uint64_t miss_byte_true[] = {4135969280, 3895592448, 3680328192, 3460513280,
                               3255544832, 2785475584, 2508606976, 2340679680};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {
      .cache_size = CACHE_SIZE, .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("SLRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = simulate_at_multi_sizes_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  print_results(cache, res);
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
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

  g_test_add_data_func("/libCacheSim/cacheAlgo_SFIFO_Merge", reader,
                       test_SFIFO_Merge);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SFIFO_Reinsertion", reader,
                       test_SFIFO_Reinsertion);

  g_test_add_data_func("/libCacheSim/cacheAlgo_LeCaR", reader, test_LeCaR);
  g_test_add_data_func("/libCacheSim/cacheAlgo_ARC", reader, test_ARC);

  g_test_add_data_func("/libCacheSim/cacheAlgo_Cacheus", reader, test_Cacheus);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LRU", reader, test_LRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SLRU", reader, test_SLRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SR_LRU", reader, test_SR_LRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_CR_LFU", reader, test_CR_LFU);

  g_test_add_data_func("/libCacheSim/cacheAlgo_Clock", reader, test_Clock);
  g_test_add_data_func("/libCacheSim/cacheAlgo_FIFO", reader, test_FIFO);
  g_test_add_data_func("/libCacheSim/cacheAlgo_MRU", reader, test_MRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Random", reader, test_Random);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LFU", reader, test_LFU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LFUDA", reader, test_LFUDA);

  g_test_add_data_func("/libCacheSim/cacheAlgo_LFUCpp", reader, test_LFUCpp);
  g_test_add_data_func("/libCacheSim/cacheAlgo_GDSF", reader, test_GDSF);

  g_test_add_data_func("/libCacheSim/cacheAlgo_LHD", reader, test_LHD);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Hyperbolic", reader,
                       test_Hyperbolic);

  /* Belady requires reader that has next access information and can only use
   * oracleGeneral trace */
  g_test_add_data_func("/libCacheSim/cacheAlgo_Belady", reader, test_Belady);
  g_test_add_data_func("/libCacheSim/cacheAlgo_BeladySize", reader,
                       test_BeladySize);

  g_test_add_data_func_full("/libCacheSim/empty", reader, empty_test,
                            test_teardown);

  return g_test_run();
}
