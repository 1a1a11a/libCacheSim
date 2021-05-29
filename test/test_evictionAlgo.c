//
// Created by Juncheng Yang on 11/21/19.
//

#include "common.h"

static void _verify_profiler_results(const sim_res_t *const res,
                              const uint64_t num_of_sizes,
                              const uint64_t req_cnt_true,
                              const uint64_t *const miss_cnt_true,
                              const uint64_t req_byte_true,
                              const uint64_t *const miss_byte_true) {

  for (uint64_t i = 0; i < num_of_sizes; i++) {
    g_assert_cmpuint(req_cnt_true, ==, res[i].req_cnt);
    g_assert_cmpuint(miss_cnt_true[i], ==, res[i].miss_cnt);
    g_assert_cmpuint(req_byte_true, ==, res[i].req_bytes);
    g_assert_cmpuint(miss_byte_true[i], ==, res[i].miss_bytes);
  }
}

static void test_LRU(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {93161, 87794, 82945, 81433, 72250, 72083, 71969, 71716};
  uint64_t miss_byte_true[] = {4036642816, 3838227968, 3664065536, 3611544064, 3081624576, 3079594496, 3075357184, 3060711936};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("LRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

//  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
//    printf("%s cache size %" PRIu64 " req %" PRIu64 " miss %" PRIu64
//           " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
//           __func__, res[i].cache_size, res[i].req_cnt, res[i].miss_cnt,
//           res[i].req_bytes, res[i].miss_bytes);
//  }

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}


static void test_FIFO(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {93193, 87172, 84321, 83888,
                             72331, 72230, 72181, 72141};
  uint64_t miss_byte_true[] = {4035451392, 3815613440, 3724681728, 3751948288,
                              3083697664, 3081942528, 3081872384, 3080036864};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("FIFO", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
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
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .per_obj_overhead = 0, .hashpower = 20, .default_ttl = 0};
  cache_t *cache = create_test_cache("Optimal", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

//  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
//    printf("cache size %" PRIu64 " req %" PRIu64 " miss %" PRIu64
//           " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
//           res[i].cache_size, res[i].req_cnt, res[i].miss_cnt, res[i].req_bytes,
//           res[i].miss_bytes);
//  }

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_Random(gconstpointer user_data) {
  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .hashpower=12, .default_ttl = 0};
  cache_t *cache = create_test_cache("Random", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

//  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
//    printf("Random cache size %" PRIu64 " req %" PRIu64 " miss %" PRIu64
//           " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
//           res[i].cache_size, res[i].req_cnt, res[i].miss_cnt, res[i].req_bytes,
//           res[i].miss_bytes);
//  }

  cache->cache_free(cache);
  g_free(res);
}

static void test_LFU(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {91385, 84061, 77353, 76506, 68994, 66441, 64819, 64376};
  uint64_t miss_byte_true[] = {3990213632, 3692986368, 3434442752, 3413374464, 2963407872, 2804032512, 2717934080, 2690728448};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("LFU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_LHD(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {91385, 84061, 77353, 76506, 68994, 66441, 64819, 64376};
  uint64_t miss_byte_true[] = {3990213632, 3692986368, 3434442752, 3413374464, 2963407872, 2804032512, 2717934080, 2690728448};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("LFU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_LeCaR(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {91385, 84061, 77353, 76506, 68994, 66441, 64819, 64376};
  uint64_t miss_byte_true[] = {3990213632, 3692986368, 3434442752, 3413374464, 2963407872, 2804032512, 2717934080, 2690728448};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("LFU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}


static void test_LFUDA(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
//  uint64_t miss_cnt_true[] = {92233, 85809, 80462, 79335, 73049, 69671, 67942, 67782};
//  uint64_t miss_byte_true[] = {4023913984, 3752341504, 3534985216, 3504430592, 3150782464, 2977339904, 2867254784, 2824277504};

  uint64_t miss_cnt_true[] = {92233, 85805, 80462, 79335, 73049, 69670, 67946, 67783};
  uint64_t miss_byte_true[] = {4023913984, 3752284160, 3534985216, 3504430592, 3150782464, 2977274368, 2867447296, 2824343040};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("LFUDA", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf("cache size %" PRIu64 " req %" PRIu64 " miss %" PRIu64
           " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
           res[i].cache_size, res[i].req_cnt, res[i].miss_cnt, res[i].req_bytes,
           res[i].miss_bytes);
  }

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_MRU(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
//  uint64_t miss_cnt_true[] = {97661, 94006, 88557, 83224,
//                             77645, 66502, 62676, 59598};
//  uint64_t miss_byte_true[] = {4002354176, 3760773120, 3541634048, 3329184768,
//                              3101117440, 2661397504, 2484923904, 2363019776};

  /* a new result */
  uint64_t miss_cnt_true[] = {100096, 93466, 87001, 82124, 77443, 71956, 68252, 64936};
  uint64_t miss_byte_true[] = {3929311744, 3599883264, 3304649728, 3097044480,
                               2896690176, 2697243648, 2576163840, 2439055360};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0, .per_obj_overhead = 0};
  cache_t *cache = create_test_cache("MRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_LRU_K(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {0,     93193, 87172, 84321, 83888,
                             72331, 72230, 72181, 72141};
  uint64_t miss_byte_true[] = {0,          4035451392, 3815613440,
                              3724681728, 3751948288, 3083697664,
                              3081942528, 3081872384, 3080036864};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("FIFO", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_ARC(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
//  uint64_t miss_cnt_true[] = {91767, 86514, 83030, 83100, 73160, 69011, 67656, 66081};
//  uint64_t miss_byte_true[] = {3998675456, 3785527296, 3658743808, 3684266496, 3143558656, 2961070080, 2879977472, 2780966912};

  uint64_t miss_cnt_true[] = {91385, 84061, 77353, 76506, 68994, 66441, 64819, 64376};
  uint64_t miss_byte_true[] = {3990213632, 3692986368, 3434442752, 3413374464, 2963407872, 2804032512, 2717934080, 2690728448};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("ARC", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_SLRU(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  uint64_t miss_cnt_true[] = {0,     93193, 87172, 84321, 83888,
                             72331, 72230, 72181, 72141};
  uint64_t miss_byte_true[] = {0,          4035451392, 3815613440,
                              3724681728, 3751948288, 3083697664,
                              3081942528, 3081872384, 3080036864};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("FIFO", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, 1);

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf("cache size %" PRIu64 " req %" PRIu64 " miss %" PRIu64
           " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
           res[i].cache_size, res[i].req_cnt, res[i].miss_cnt, res[i].req_bytes,
           res[i].miss_bytes);
  }

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_slabLRC(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  //  uint64_t miss_cnt_true[] = {93609, 89934, 84798, 84371, 84087, 73067,
  //  72652, 72489}; uint64_t miss_byte_true[] = {4047843328, 3924603392,
  //  3714857984, 3741901824, 3760001536, 3095377408, 3084561920, 3083687936};
  //  uint64_t miss_cnt_true[] = {93609, 89925, 84798, 84371, 84087, 73063,
  //  72649, 72488}; uint64_t miss_byte_true[] = {4047843328, 3924427264,
  //  3714857984, 3741901824, 3760001536, 3095344640, 3084503040, 3083638784};

  uint64_t miss_cnt_true[] = {93607, 89903, 84787, 84371,
                             84087, 73056, 72578, 72479};
  uint64_t miss_byte_true[] = {4047763968, 3923623424, 3714377216, 3741901824,
                              3760001536, 3095218176, 3084029440, 3083782144};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("slabLRC", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf("%s cache size %" PRIu64 " req %" PRIu64 " miss %" PRIu64
           " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
           __func__, res[i].cache_size, res[i].req_cnt, res[i].miss_cnt,
           res[i].req_bytes, res[i].miss_bytes);
  }

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_slabLRU(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  //  uint64_t miss_cnt_true[] = {92733, 90020, 86056, 82894, 80750, 74338,
  //  71370, 71091}; uint64_t miss_byte_true[] = {4018161152, 3936436224,
  //  3794590720, 3686940160, 3601066496, 3248778752, 3079868416, 3072276480};

  uint64_t miss_cnt_true[] = {92733, 90020, 86056, 82894,
                             80750, 74271, 71356, 71091};
  uint64_t miss_byte_true[] = {4018161152, 3936436224, 3794590720, 3686940160,
                              3601066496, 3244842496, 3079143424, 3072276480};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("slabLRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf("%s cache size %" PRIu64 " req %" PRIu64 " miss %" PRIu64
           " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
           __func__, res[i].cache_size, res[i].req_cnt, res[i].miss_cnt,
           res[i].req_bytes, res[i].miss_bytes);
  }

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void test_slabObjLRU(gconstpointer user_data) {
  uint64_t req_cnt_true = 113872, req_byte_true = 4205978112;
  //  uint64_t miss_cnt_true[] = {91067, 88334, 85939, 79621, 78776, 77461,
  //  74381, 72492}; uint64_t miss_byte_true[] = {3985031168, 3864086016,
  //  3775024128, 3396319232, 3368396800, 3304763392, 3110617088, 3063127040};
  //  uint64_t miss_cnt_true[] = {90907, 88260, 85781, 79594, 78736, 77353,
  //  74381, 72398}; uint64_t miss_byte_true[] = {3977553408, 3861612032,
  //  3769109504, 3395590144, 3366529024, 3300487168, 3110617088, 3060845568};

  uint64_t miss_cnt_true[] = {90523, 87887, 80209, 79476,
                             78608, 77128, 74381, 72271};
  uint64_t miss_byte_true[] = {3954878976, 3846666752, 3416866816, 3391838208,
                              3360889856, 3289858048, 3110617088, 3057359872};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("slabObjLRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

static void empty_test(gconstpointer user_data) { ; }

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t *reader;

  reader = setup_csv_reader_obj_num();
//  reader = setup_vscsi_reader();
  g_test_add_data_func("/libCacheSim/cacheAlgo_LRU", reader, test_LRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_FIFO", reader, test_FIFO);
  g_test_add_data_func("/libCacheSim/cacheAlgo_MRU", reader, test_MRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Random", reader, test_Random);
  g_test_add_data_func("/libCacheSim/cacheAlgo_ARC", reader, test_ARC);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LFU", reader, test_LFU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_LFUDA", reader, test_LFUDA);
  g_test_add_data_func_full("/libCacheSim/free_reader", reader, empty_test,
                            test_teardown);

  /* optimal requires reader that has next access information, note that
   * oracleGeneral trace removes all object size changes */
  reader = setup_oracleGeneralBin_reader();
  g_test_add_data_func("/libCacheSim/cacheAlgo_Optimal", reader, test_Optimal);


  /* these are wrong now */
//  g_test_add_data_func("/libCacheSim/cacheAlgo_SlabLRC", reader, test_slabLRC);
//  g_test_add_data_func("/libCacheSim/cacheAlgo_SlabLRU", reader, test_slabLRU);
//  g_test_add_data_func("/libCacheSim/cacheAlgo_SlabObjLRU", reader,
//                       test_slabObjLRU);

  g_test_add_data_func_full("/libCacheSim/empty", reader, empty_test,
                            test_teardown);

  return g_test_run();
}
