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
    g_assert_cmpuint(miss_cnt_true[i], ==, res[i].n_miss);
    g_assert_cmpuint(req_byte_true, ==, res[i].n_req_byte);
    g_assert_cmpuint(miss_byte_true[i], ==, res[i].n_miss_byte);
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

static void test_LRU(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {93161, 87794, 82945, 81433, 72250, 72083, 71969, 71716};
  uint64_t miss_byte_true[] = {4036642816, 3838227968, 3664065536, 3611544064,
                               3081624576, 3079594496, 3075357184, 3060711936};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Clock(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {91385, 84061, 77353, 76506, 68994, 66441, 64819, 64376};
  uint64_t miss_byte_true[] = {3990213632, 3692986368, 3434442752, 3413374464,
                               2963407872, 2804032512, 2717934080, 2690728448};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Clock", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}


static void test_FIFO(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {93193, 87172, 84321, 83888,
                             72331, 72230, 72181, 72141};
  uint64_t miss_byte_true[] = {4035451392, 3815613440, 3724681728, 3751948288,
                              3083697664, 3081942528, 3081872384, 3080036864};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("FIFO", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Optimal(gconstpointer user_data) {
  /* the request byte is different from others because the oracleGeneral
   * trace removes all object size changes (and use the size of last appearance
   * of an object as the object size throughout the trace */
  uint64_t req_cnt_true = 113872, req_byte_true = 4368040448;
  uint64_t miss_cnt_true[] = {79256, 70724, 65481, 61594, 59645, 57599, 50873, 48974};
  uint64_t miss_byte_true[] = {3472532480, 2995165696, 2726689792, 2537648128,
                               2403427840, 2269212672, 2134992896, 2029769728};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
                                     .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Optimal", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_OptimalSize(gconstpointer user_data) {
  /* the request byte is different from others because the oracleGeneral
   * trace removes all object size changes (and use the size of last appearance
   * of an object as the object size throughout the trace */
  uint64_t req_cnt_true = 113872, req_byte_true = 4368040448;
  uint64_t miss_cnt_true[] = {74329, 64524, 60279, 56514, 54539, 52613, 50581, 48974};
  uint64_t miss_byte_true[] = {3507168256, 3044453888, 2773635072, 2537643008, 
                               2403463680, 2269248512, 2135011840, 2029769728};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("OptimalSize", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Random(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {92056, 87857, 83304, 78929, 74241, 70337, 66402, 62829};
  uint64_t miss_byte_true[] = {3990722048, 3814795264, 3595883008, 3379089920,
                               3191873024, 2987283968, 2793013248, 2621837824};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .hashpower=12,
                                     .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Random", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LFUFast(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {91385, 84061, 77353, 76506, 68994, 66441, 64819, 64376};
  uint64_t miss_byte_true[] = {3990213632, 3692986368, 3434442752, 3413374464,
                               2963407872, 2804032512, 2717934080, 2690728448};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LFUFast", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_LFUCpp(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {91385, 84061, 77353, 76506, 68994, 66441, 64819, 64376};
  uint64_t miss_byte_true[] = {3990213632, 3692986368, 3434442752, 3413374464,
                               2963407872, 2804032512, 2717934080, 2690728448};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LFU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_GDSF(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {86056, 82265, 77939, 68727, 67636, 63616, 58678, 58465};
  uint64_t miss_byte_true[] = {3863223808, 3707204608, 3552397312, 3267907584,
                               3198254592, 2966758400, 2666385408, 2652331008};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("GDSF", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LHD(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {88907, 84394, 80846, 76482, 70960, 66571, 63627, 61156};
  uint64_t miss_byte_true[] = {3931543552, 3755170304, 3620246528, 3455476736,
                               3240826880, 3055610368, 2918132736, 2789520896};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LHD", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  // print_results(cache, res); 
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Hyperbolic(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {85819, 78714, 70297, 64546, 61234, 57377, 56121, 54930};
  uint64_t miss_byte_true[] = {3832728576, 3443996160, 3132970496, 3008947712,
                               2823863808, 2587067392, 2510083584, 2432542208};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Hyperbolic", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LeCaR(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  // uint64_t miss_cnt_true[] = {93210, 86935, 78841, 81401, 72380, 68472, 65219, 68423};
  // uint64_t miss_byte_true[] = {4040029696, 3800505344, 3511674368, 3609508352, 
  //                             3090144256, 2904426496, 2736208384, 2849179136};
  uint64_t miss_cnt_true[] = {93081, 85767, 81649, 81366, 72035, 68266, 67224, 68577};
  uint64_t miss_byte_true[] = {4032769024, 3756328960, 3597538304, 3609532928, 
                               3059255808, 2920182272, 2832361984, 2857644032};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LeCaR", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  // print_results(cache, res); 
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_Cacheus(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {89422, 83826, 80091, 72648, 69286, 67883, 67476, 66501};
  uint64_t miss_byte_true[] = {3907576320, 3663195136, 3441767424, 3147611648, 
                              2962097152, 2885015040, 2864854528, 2807194112};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("Cacheus", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_SR_LRU(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {89493, 83770, 81321, 76521, 72708, 71775, 67375, 65728};
  uint64_t miss_byte_true[] = {3925441024, 3711699456, 3518652928, 3296385024,
                               3152901120, 3038793216, 2867397120, 2769311232};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("SR_LRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_CR_LFU(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {92084, 88226, 84701, 80430, 75909, 66138, 59767, 54763};
  uint64_t miss_byte_true[] = {3972743680, 3726964736, 3516974592, 3308064768,
                               3078354432, 2621426176, 2372660736, 2297313280};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("CR_LFU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}


static void test_LFUDA(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {92233, 85805, 80462, 79335, 73049, 69670, 67946, 67783};
  uint64_t miss_byte_true[] = {4023913984, 3752284160, 3534985216, 3504430592,
                               3150782464, 2977274368, 2867447296, 2824343040};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("LFUDA", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_MRU(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  /* a new result */
  uint64_t miss_cnt_true[] = {100096, 93466, 87001, 82124, 77443, 71956, 68252, 64936};
  uint64_t miss_byte_true[] = {3929311744, 3599883264, 3304649728, 3097044480,
                               2896690176, 2697243648, 2576163840, 2439055360};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("MRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_LRU_K(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {0,     93193, 87172, 84321, 83888,
                             72331, 72230, 72181, 72141};
  uint64_t miss_byte_true[] = {0,          4035451392, 3815613440,
                              3724681728, 3751948288, 3083697664,
                              3081942528, 3081872384, 3080036864};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("FIFO", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_ARC(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {93798, 87602, 84471, 83216, 73606, 68674, 72144, 72726};
  uint64_t miss_byte_true[] = {4037066752, 3814765056, 3722742784, 3637225984,
                               3158606848, 2914956800, 3077340160, 3112387584};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("ARC", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}

static void test_SLRU(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {89290, 86852, 81287, 76035, 70552, 62053, 59286, 56604};
  uint64_t miss_byte_true[] = {3902296576, 3722598912, 3461846016, 3240596992,
                               3022791680, 2708081152, 2618411008, 2460068352};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE,
      .hashpower = 20, .default_ttl = DEFAULT_TTL};
  cache_t *cache = create_test_cache("SLRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  cache_stat_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 0, _n_cores());
  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  my_free(sizeof(cache_stat_t), res);
}


static void empty_test(gconstpointer user_data) { ; }

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  srand(0); // for reproducibility
  reader_t *reader;

  reader = setup_csv_reader_obj_num();
//  reader = setup_vscsi_reader();
  g_test_add_data_func("/libCacheSim/cacheAlgo_LeCaR", reader, test_LeCaR);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Cacheus", reader, test_Cacheus);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LRU", reader, test_LRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SLRU", reader, test_SLRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SR_LRU", reader, test_SR_LRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_CR_LFU", reader, test_CR_LFU);

  g_test_add_data_func("/libCacheSim/cacheAlgo_Clock", reader, test_Clock);
  g_test_add_data_func("/libCacheSim/cacheAlgo_FIFO", reader, test_FIFO);
  g_test_add_data_func("/libCacheSim/cacheAlgo_MRU", reader, test_MRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Random", reader, test_Random);
  g_test_add_data_func("/libCacheSim/cacheAlgo_ARC", reader, test_ARC);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LFUFast", reader, test_LFUFast);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LFUDA", reader, test_LFUDA);

  g_test_add_data_func("/libCacheSim/cacheAlgo_LFU", reader, test_LFUCpp);
  g_test_add_data_func("/libCacheSim/cacheAlgo_GDSF", reader, test_GDSF);

  g_test_add_data_func("/libCacheSim/cacheAlgo_LHD", reader, test_LHD);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Hyperbolic", reader, test_Hyperbolic);
  g_test_add_data_func_full("/libCacheSim/free_reader", reader, empty_test, test_teardown);

  /* optimal requires reader that has next access information, note that
   * oracleGeneral trace removes all object size changes */
  reader = setup_oracleGeneralBin_reader();
  g_test_add_data_func("/libCacheSim/cacheAlgo_Optimal", reader, test_Optimal);
  g_test_add_data_func("/libCacheSim/cacheAlgo_OptimalSize", reader, test_OptimalSize);

  g_test_add_data_func_full("/libCacheSim/empty", reader, empty_test,
                            test_teardown);

  return g_test_run();
}
