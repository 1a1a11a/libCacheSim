//
// Created by Juncheng Yang on 11/21/19.
//

#include "test_common.h"


void test_profiler_basic(gconstpointer user_data) {
  guint64 req_cnt_true[] = {0, 113872, 113872, 113872, 113872, 113872};
  guint64 miss_cnt_true[] = {113872, 102640, 100215, 98718, 97074};
//  guint64 req_byte_true[] = {0, 4205978112, 4205978112, 4205978112, 4205978112, 4205978112};
//  guint64 miss_byte_true[] = {4205978112, 4146084864, 4135202816, 4128339456, 4120576000, };

  reader_t *reader = (reader_t *) user_data;
  cache_t *cache = create_cache("LRUSize", CACHE_SIZE, reader->base->obj_id_type, NULL);
//  cache_t* cache2 = FIFOSize_init(CACHE_SIZE, reader->base->obj_id_type, NULL);
//  cache_t *cache = create_cache("LRUSize", CACHE_SIZE, reader->base->obj_id_type, NULL);
//  cache_t *cache = create_cache_internal("LRU", CACHE_SIZE, reader->base->obj_id_type, NULL);
  g_assert_true(cache != NULL);
  profiler_res_t *res = get_miss_ratio_curve(reader, cache, 4, BIN_SIZE);

  guint64* mc = _get_lru_miss_cnt_seq(reader, get_num_of_req(reader));
  for (int i=0; i<CACHE_SIZE/BIN_SIZE+1; i++){
    printf("%lld req %lld miss %lld req_byte %lld miss_byte %lld\n", res[i].cache_size, res[i].req_cnt, res[i].miss_cnt, res[i].req_byte, res[i].miss_byte);
  }


  g_assert_cmpuint(res[0].cache_size, ==, 0);
  g_assert_cmpuint(res[0].req_cnt, ==, 0);
  g_assert_cmpuint(res[0].miss_cnt, ==, 0);

  for (int i = 1; i < CACHE_SIZE / BIN_SIZE + 1; i++) {
    printf("%ld %ld %ld\n", req_cnt_true[i], BIN_SIZE*i, res[i].req_cnt);
    g_assert_cmpuint(req_cnt_true[i], ==, res[i].req_cnt);
    g_assert_cmpuint(miss_cnt_true[i], ==, res[i].miss_cnt);
//    g_assert_cmpuint(req_byte_true[i], ==, res[i].req_byte);
//    g_assert_cmpuint(miss_byte_true[i], ==, res[i].miss_byte);
  }
  cache->core->destroy(cache);
  g_free(res);
}


int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t *reader;

  reader = setup_plaintxt_reader_num();
  g_test_add_data_func("/libmimircache/test_profiler_basic_plain_num", reader, test_profiler_basic);
//  g_test_add_data_func_full("/libmimircache/test_profiler_more1_plain_num", reader, test_profiler_more1, test_teardown);

  reader = setup_plaintxt_reader_str();
  g_test_add_data_func("/libmimircache/test_profiler_basic_plain_str", reader, test_profiler_basic);
//  g_test_add_data_func_full("/libmimircache/test_profiler_more1_plain_str", reader, test_profiler_more1, test_teardown);


  reader = setup_csv_reader_obj_num();
  g_test_add_data_func("/libmimircache/test_profiler_basic_csv_num", reader, test_profiler_basic);
//  g_test_add_data_func_full("/libmimircache/test_profiler_more1_csv_num", reader, test_profiler_more1,
//                            test_teardown);

  reader = setup_csv_reader_obj_str();
  g_test_add_data_func("/libmimircache/test_profiler_basic_csv_str", reader, test_profiler_basic);
//  g_test_add_data_func_full("/libmimircache/test_profiler_more1_csv_str", reader, test_profiler_more1,
//                            test_teardown);

  reader = setup_binary_reader();
  g_test_add_data_func("/libmimircache/test_profiler_basic_binary", reader, test_profiler_basic);
//  g_test_add_data_func_full("/libmimircache/test_profiler_more1_binary", reader, test_profiler_more1, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func("/libmimircache/test_profiler_basic_vscsi", reader, test_profiler_basic);
//  g_test_add_data_func_full("/libmimircache/test_profiler_more1_vscsi", reader, test_profiler_more1, test_teardown);

  return g_test_run();
}