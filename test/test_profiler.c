//
// Created by Juncheng Yang on 11/21/19.
//

#include "test_common.h"

/**
 * this one for testing with the plain trace reader, which does not have obj size information
 * @param user_data
 */
void test_profiler_no_size(gconstpointer user_data) {
  guint64 cache_size = CACHE_SIZE/MB;
  guint64 step_size = STEP_SIZE/MB;

  guint64 req_cnt_true = 113872;
  guint64 miss_cnt_true[] = {99411, 96397, 95652, 95370, 95182, 94997, 94891, 94816};

  reader_t *reader = (reader_t *) user_data;
  common_cache_params_t cc_params = {.cache_size=cache_size, .obj_id_type=reader->base->obj_id_type, .default_ttl=0};
  cache_t *cache = create_cache("LRU", cc_params, NULL);
  g_assert_true(cache != NULL);
  profiler_res_t *res = get_miss_ratio_curve_with_step_size(reader, cache, step_size, NULL, 0, 4);

//  guint64* mc = _get_lru_miss_cnt(reader, get_num_of_req(reader));

  for (int i = 0; i < cache_size / step_size; i++) {
    g_assert_cmpuint(res[i].cache_size, ==, step_size*(i+1));
    g_assert_cmpuint(req_cnt_true, ==, res[i].req_cnt);
    g_assert_cmpuint(miss_cnt_true[i], ==, res[i].miss_cnt);
    g_assert_cmpuint(req_cnt_true, ==, res[i].req_byte);
    g_assert_cmpuint(miss_cnt_true[i], ==, res[i].miss_byte);
  }
  cache->core.cache_free(cache);
  g_free(res);
}


/**
 * this one for testing with all other cache readers, which contain obj size information
 * @param user_data
 */
void test_profiler(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true =4205978112;
  guint64 miss_cnt_true[] = {93161, 87795, 82945, 81433, 72250, 72083, 71969, 71716};
  guint64 miss_byte_true[] = {4036642816, 3838236160, 3664065536, 3611544064,
                              3081624576, 3079594496, 3075357184, 3060711936};

  reader_t *reader = (reader_t *) user_data;
  common_cache_params_t cc_params = {.cache_size=CACHE_SIZE, .obj_id_type=reader->base->obj_id_type, .default_ttl=0};
  cache_t *cache = create_cache("LRU", cc_params, NULL);
  g_assert_true(cache != NULL);


  profiler_res_t *res = get_miss_ratio_curve_with_step_size(reader, cache, STEP_SIZE, NULL, 0, 4);
  for (int i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    g_assert_cmpuint(res[i].cache_size, ==, STEP_SIZE*(i+1));
    g_assert_cmpuint(res[i].req_cnt,  ==, req_cnt_true);
    g_assert_cmpuint(res[i].miss_cnt, ==, miss_cnt_true[i]);
    g_assert_cmpuint(res[i].req_byte, ==, req_byte_true);
    g_assert_cmpuint(res[i].miss_byte, ==, miss_byte_true[i]);
  }
  g_free(res);


  guint64 cache_sizes[] = {STEP_SIZE, STEP_SIZE*2, STEP_SIZE*4, STEP_SIZE*7};
  res = get_miss_ratio_curve(reader, cache, 4, cache_sizes, NULL, 0, 4);
  g_assert_cmpuint(res[0].cache_size, ==, STEP_SIZE);
  g_assert_cmpuint(res[1].req_byte, ==, req_byte_true);
  g_assert_cmpuint(res[3].req_cnt, ==, req_cnt_true);

  g_assert_cmpuint(res[0].miss_byte, ==, miss_byte_true[0]);
  g_assert_cmpuint(res[2].miss_cnt, ==, miss_cnt_true[3]);
  g_assert_cmpuint(res[3].miss_byte, ==, miss_byte_true[6]);
  g_free(res);

  cache->core.cache_free(cache);
}


/**
 * this one for testing warmup
 * @param user_data
 */
void test_profiler_with_warmup1(gconstpointer user_data) {
  guint64 req_cnt_true = 113872, req_byte_true =4205978112;
  guint64 miss_cnt_true[] = {93009, 87634, 82782, 81267, 72080, 71911, 71770, 71333};
  guint64 miss_byte_true[] = {4034876416, 3836416512, 3662237696, 3609691648, 3079616512, 3077579776, 3072181760, 3045314048};

  reader_t *reader = (reader_t *) user_data;
  common_cache_params_t cc_params = {.cache_size=CACHE_SIZE, .obj_id_type=reader->base->obj_id_type, .default_ttl=0};
  cache_t *cache = create_cache("LRU", cc_params, NULL);
  g_assert_true(cache != NULL);

  profiler_res_t *res = get_miss_ratio_curve_with_step_size(reader, cache, STEP_SIZE, reader, 0, 4);
//  for (int i=0; i<CACHE_SIZE/STEP_SIZE; i++){
//    printf("size %lld - req %lld - miss %lld - req_byte %lld - miss_byte %lld\n",
//        res[i].cache_size, res[i].req_cnt, res[i].miss_cnt, res[i].req_byte, res[i].miss_byte);
//  }

  for (int i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    g_assert_cmpuint(res[i].cache_size, ==, STEP_SIZE*(i+1));
    g_assert_cmpuint(res[i].req_cnt,  ==, req_cnt_true);
    g_assert_cmpuint(res[i].miss_cnt, ==, miss_cnt_true[i]);
    g_assert_cmpuint(res[i].req_byte, ==, req_byte_true);
    g_assert_cmpuint(res[i].miss_byte, ==, miss_byte_true[i]);
  }
  g_free(res);

  cache->core.cache_free(cache);
}


void test_profiler_with_warmup2(gconstpointer user_data) {
  guint64 req_cnt_true = 91098, req_byte_true = 3180282368;
  guint64 miss_cnt_true[] = {75028, 69711, 65084, 63574, 57253, 57101, 56987, 56734};
  guint64 miss_byte_true[] = {3036331008, 2839408640, 2676338688, 2623825408, 2263264256, 2261316096, 2257078784, 2242433536};

  reader_t *reader = (reader_t *) user_data;
  common_cache_params_t cc_params = {.cache_size=CACHE_SIZE, .obj_id_type=reader->base->obj_id_type, .default_ttl=0};
  cache_t *cache = create_cache("LRU", cc_params, NULL);
  g_assert_true(cache != NULL);

  profiler_res_t *res = get_miss_ratio_curve_with_step_size(reader, cache, STEP_SIZE, NULL, 0.2, 4);
//  for (int i=0; i<CACHE_SIZE/STEP_SIZE; i++){
//    printf("size %lld - req %lld - miss %lld - req_byte %lld - miss_byte %lld\n",
//           res[i].cache_size, res[i].req_cnt, res[i].miss_cnt, res[i].req_byte, res[i].miss_byte);
//  }

  for (int i = 0; i < CACHE_SIZE / STEP_SIZE; i++) {
    g_assert_cmpuint(res[i].cache_size, ==, STEP_SIZE*(i+1));
    g_assert_cmpuint(res[i].req_cnt,  ==, req_cnt_true);
    g_assert_cmpuint(res[i].miss_cnt, ==, miss_cnt_true[i]);
    g_assert_cmpuint(res[i].req_byte, ==, req_byte_true);
    g_assert_cmpuint(res[i].miss_byte, ==, miss_byte_true[i]);
  }
  g_free(res);

  cache->core.cache_free(cache);
}


int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t *reader;

//  reader = setup_plaintxt_reader_num();
//  g_test_add_data_func_full("/libmimircache/test_profiler_no_size_plain_num", reader, test_profiler_no_size, test_teardown);
//
//  reader = setup_plaintxt_reader_str();
//  g_test_add_data_func_full("/libmimircache/test_profiler_no_size_plain_str", reader, test_profiler_no_size, test_teardown);
//
//  reader = setup_csv_reader_obj_num();
//  g_test_add_data_func_full("/libmimircache/test_profiler_csv_num", reader, test_profiler, test_teardown);
//
//  reader = setup_csv_reader_obj_str();
//  g_test_add_data_func_full("/libmimircache/test_profiler_csv_str", reader, test_profiler, test_teardown);
//
//  reader = setup_binary_reader();
//  g_test_add_data_func_full("/libmimircache/test_profiler_binary", reader, test_profiler, test_teardown);
//
//  reader = setup_vscsi_reader();
//  g_test_add_data_func_full("/libmimircache/test_profiler_vscsi", reader, test_profiler, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func_full("/libmimircache/test_profiler_warmup1", reader, test_profiler_with_warmup1, test_teardown);
  reader = setup_vscsi_reader();
  g_test_add_data_func_full("/libmimircache/test_profiler_warmup2", reader, test_profiler_with_warmup2, test_teardown);

  return g_test_run();
}