//
// Created by Juncheng Yang on 11/21/19.
//

#include "common.h"

void _verify_profiler_results(const sim_res_t *const res,
                              const guint64 num_of_sizes,
                              const guint64 req_cnt_true,
                              const guint64 *const miss_cnt_true,
                              const guint64 req_byte_true,
                              const guint64 *const miss_byte_true) {

  for (uint64_t i = 0; i < num_of_sizes; i++) {
    g_assert_cmpuint(req_cnt_true, ==, res[i].req_cnt);
    g_assert_cmpuint(miss_cnt_true[i], ==, res[i].miss_cnt);
    g_assert_cmpuint(req_byte_true, ==, res[i].req_bytes);
    g_assert_cmpuint(miss_byte_true[i], ==, res[i].miss_bytes);
  }
}

void test_FIFO(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true = 4205978112;
  guint64 miss_cnt_true[] = {93193, 87172, 84321, 83888,
                             72331, 72230, 72181, 72141};
  guint64 miss_byte_true[] = {4035451392, 3815613440, 3724681728, 3751948288,
                              3083697664, 3081942528, 3081872384, 3080036864};

  // what are these???
  //  guint64 miss_cnt_true[] = {93185, 87058, 84316, 84005, 72491, 72228,
  //  72182, 72140}; guint64 miss_byte_true[] = {4034701824, 3806639104,
  //  3717263360, 3750604800, 3091751936, 3081934336, 3081873920, 3080034816};
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

void test_Optimal(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true = 4205978112;
  guint64 miss_cnt_true[] = {0,     93193, 87172, 84321, 83888,
                             72331, 72230, 72181, 72141};
  guint64 miss_byte_true[] = {0,          4035451392, 3815613440,
                              3724681728, 3751948288, 3083697664,
                              3081942528, 3081872384, 3080036864};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("FIFO", cc_params, reader, NULL);
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

void test_Random(gconstpointer user_data) {
  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("Random", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  for (uint64_t i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    printf("Random cache size %" PRIu64 " req %" PRIu64 " miss %" PRIu64
           " req_bytes %" PRIu64 " miss_bytes %" PRIu64 "\n",
           res[i].cache_size, res[i].req_cnt, res[i].miss_cnt, res[i].req_bytes,
           res[i].miss_bytes);
  }

  cache->cache_free(cache);
  g_free(res);
}

void test_LFU(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true = 4205978112;
  guint64 miss_cnt_true[] = {0,     93193, 87172, 84321, 83888,
                             72331, 72230, 72181, 72141};
  guint64 miss_byte_true[] = {0,          4035451392, 3815613440,
                              3724681728, 3751948288, 3083697664,
                              3081942528, 3081872384, 3080036864};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("FIFO", cc_params, reader, NULL);
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

void test_MRU(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true = 4205978112;
  guint64 miss_cnt_true[] = {97661, 94006, 88557, 83224,
                             77645, 66502, 62676, 59598};
  guint64 miss_byte_true[] = {4002354176, 3760773120, 3541634048, 3329184768,
                              3101117440, 2661397504, 2484923904, 2363019776};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("MRU", cc_params, reader, NULL);
  g_assert_true(cache != NULL);
  sim_res_t *res = get_miss_ratio_curve_with_step_size(
      reader, cache, STEP_SIZE, NULL, 0, NUM_OF_THREADS);

  _verify_profiler_results(res, CACHE_SIZE / STEP_SIZE, req_cnt_true,
                           miss_cnt_true, req_byte_true, miss_byte_true);
  cache->cache_free(cache);
  g_free(res);
}

void test_LRU_K(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true = 4205978112;
  guint64 miss_cnt_true[] = {0,     93193, 87172, 84321, 83888,
                             72331, 72230, 72181, 72141};
  guint64 miss_byte_true[] = {0,          4035451392, 3815613440,
                              3724681728, 3751948288, 3083697664,
                              3081942528, 3081872384, 3080036864};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("FIFO", cc_params, reader, NULL);
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

void test_ARC(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true = 4205978112;
  guint64 miss_cnt_true[] = {0,     93193, 87172, 84321, 83888,
                             72331, 72230, 72181, 72141};
  guint64 miss_byte_true[] = {0,          4035451392, 3815613440,
                              3724681728, 3751948288, 3083697664,
                              3081942528, 3081872384, 3080036864};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("FIFO", cc_params, reader, NULL);
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

void test_SLRU(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true = 4205978112;
  guint64 miss_cnt_true[] = {0,     93193, 87172, 84321, 83888,
                             72331, 72230, 72181, 72141};
  guint64 miss_byte_true[] = {0,          4035451392, 3815613440,
                              3724681728, 3751948288, 3083697664,
                              3081942528, 3081872384, 3080036864};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("FIFO", cc_params, reader, NULL);
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

void test_slabLRC(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true = 4205978112;
  //  guint64 miss_cnt_true[] = {93609, 89934, 84798, 84371, 84087, 73067,
  //  72652, 72489}; guint64 miss_byte_true[] = {4047843328, 3924603392,
  //  3714857984, 3741901824, 3760001536, 3095377408, 3084561920, 3083687936};
  //  guint64 miss_cnt_true[] = {93609, 89925, 84798, 84371, 84087, 73063,
  //  72649, 72488}; guint64 miss_byte_true[] = {4047843328, 3924427264,
  //  3714857984, 3741901824, 3760001536, 3095344640, 3084503040, 3083638784};

  guint64 miss_cnt_true[] = {93607, 89903, 84787, 84371,
                             84087, 73056, 72578, 72479};
  guint64 miss_byte_true[] = {4047763968, 3923623424, 3714377216, 3741901824,
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

void test_slabLRU(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true = 4205978112;
  //  guint64 miss_cnt_true[] = {92733, 90020, 86056, 82894, 80750, 74338,
  //  71370, 71091}; guint64 miss_byte_true[] = {4018161152, 3936436224,
  //  3794590720, 3686940160, 3601066496, 3248778752, 3079868416, 3072276480};

  guint64 miss_cnt_true[] = {92733, 90020, 86056, 82894,
                             80750, 74271, 71356, 71091};
  guint64 miss_byte_true[] = {4018161152, 3936436224, 3794590720, 3686940160,
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

void test_slabObjLRU(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true = 4205978112;
  //  guint64 miss_cnt_true[] = {91067, 88334, 85939, 79621, 78776, 77461,
  //  74381, 72492}; guint64 miss_byte_true[] = {3985031168, 3864086016,
  //  3775024128, 3396319232, 3368396800, 3304763392, 3110617088, 3063127040};
  //  guint64 miss_cnt_true[] = {90907, 88260, 85781, 79594, 78736, 77353,
  //  74381, 72398}; guint64 miss_byte_true[] = {3977553408, 3861612032,
  //  3769109504, 3395590144, 3366529024, 3300487168, 3110617088, 3060845568};

  guint64 miss_cnt_true[] = {90523, 87887, 80209, 79476,
                             78608, 77128, 74381, 72271};
  guint64 miss_byte_true[] = {3954878976, 3846666752, 3416866816, 3391838208,
                              3360889856, 3289858048, 3110617088, 3057359872};

  reader_t *reader = (reader_t *)user_data;
  common_cache_params_t cc_params = {.cache_size = CACHE_SIZE, .default_ttl = 0};
  cache_t *cache = create_test_cache("slabObjLRU", cc_params, reader, NULL);
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

void empty_test(gconstpointer user_data) { ; }

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t *reader;

  reader = setup_csv_reader_obj_num();
  //  reader = setup_vscsi_reader();
  g_test_add_data_func("/libCacheSim/cacheAlgo_FIFO", reader, test_FIFO);
  g_test_add_data_func("/libCacheSim/cacheAlgo_MRU", reader, test_MRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_Random", reader, test_Random);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SlabLRC", reader, test_slabLRC);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SlabLRU", reader, test_slabLRU);
  g_test_add_data_func("/libCacheSim/cacheAlgo_SlabObjLRU", reader,
                       test_slabObjLRU);

  g_test_add_data_func_full("/libCacheSim/empty", reader, empty_test,
                            test_teardown);

  return g_test_run();
}
