//
// Created by Juncheng Yang on 11/21/19.
//

#include "test_common.h"


void test_profiler_basic(gconstpointer user_data) {
  reader_t *reader = (reader_t *) user_data;
  guint64 *hc_lruprofiler = get_hit_count_seq(reader, -1);

  cache_t *cache = create_cache_internal("LRU", CACHE_SIZE, reader->base->obj_id_type, NULL);
  profiler_res_t **res = run_trace(reader, cache, 4, (int) BIN_SIZE);
  for (int i = 0; i < CACHE_SIZE / BIN_SIZE + 1; i++) {
    g_assert_cmpuint(hc_lruprofiler[BIN_SIZE*i], ==, res[i]->req_cnt-res[i]->miss_cnt);
    g_free(res[i]);
  }
  cache->core->destroy(cache);
  g_free(res);
}

void test_profiler_more1(gconstpointer user_data) {
  reader_t *reader = (reader_t *) user_data;
  cache_t *cache = create_cache_internal("LRU", CACHE_SIZE, reader->base->obj_id_type, NULL);

  profiler_res_t **res = run_trace(reader, cache, 4, (int) BIN_SIZE);
  for (int i = 0; i < CACHE_SIZE / BIN_SIZE + 1; i++) {
//    printf("%d %d\n", i, res[i]->)
//    g_assert_cmpuint(hc_lruprofiler[BIN_SIZE*i], ==, res[i]->hit_count);
    g_free(res[i]);


    // this should be moved to LRUProfiler
//    _get_last_access_dist_seq()
//    get_bp_rtime()
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