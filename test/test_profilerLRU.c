//
// Created by Juncheng Yang on 11/21/19.
//


#include "test_common.h"


void test_profilerLRU_basic(gconstpointer user_data) {
  // note LRU profiler does not support variable size request
  reader_t* reader = (reader_t*) user_data;
  long i;
  double *hr, *mr;
  guint64 *hc;

  double hr_true[N_TEST] = {0, 0.023579, 0.029393, 0.034319, 0.040976, 0.043066, };
  double hr_last_size20_true = 0.072985, hr_last_true = 0.569921;
  guint64 hc_true[N_TEST] = {0, 2685, 3347, 3908, 4666, 4904};

  double omr_true[N_TEST] = {1, 0.976421, 0.970607, 0.965681, 0.959024, 0.956934};
  double bmr_true[N_TEST] = {};


  hr = get_lru_hit_ratio_seq(reader, 20);
  for (i=0; i<N_TEST; i++) {
    g_assert_cmpfloat(fabs(hr[i]-hr_true[i]), <=, 0.0001);
  }
  g_free(hr);

  g_assert_cmpfloat(fabs(hr[20]-hr_last_size20_true), <=, 0.0001);

  hr = get_lru_hit_ratio_seq(reader, -1);
  for (i=0; i<N_TEST; i++) {
    g_assert_cmpfloat(fabs(hr[i]-hr_true[i]), <=, 0.0001);
  }
  g_assert_cmpfloat(fabs(hr[get_num_of_req(reader)-1]-hr_last_true), <=, 0.0001);
  g_free(hr);

  hc = get_lru_hit_count_seq(reader, -1);
  for (i=0; i<N_TEST; i++) {
    g_assert_cmpuint(hc[i], ==, hc_true[i]);
  }
  g_free(hc);

  mr = get_lru_obj_miss_ratio_seq(reader, -1);
  for (i=0; i<N_TEST; i++) {
//    printf("%ld %lf - %lf\n", i, mr[i], omr_true[i]);
    g_assert_cmpfloat(fabs(mr[i]-omr_true[i]), <=, 0.0001);
  }
  g_free(mr);
}

void test_profilerLRU_more1(gconstpointer user_data){
  reader_t* reader = (reader_t*) user_data;

}


int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t *reader;

  reader = setup_plaintxt_reader_num();
  g_test_add_data_func("/libmimircache/test_profilerLRU_basic_plain_num", reader, test_profilerLRU_basic);
  g_test_add_data_func_full("/libmimircache/test_profilerLRU_more1_plain_num", reader, test_profilerLRU_more1, test_teardown);

  reader = setup_plaintxt_reader_str();
  g_test_add_data_func("/libmimircache/test_profilerLRU_basic_plain_str", reader, test_profilerLRU_basic);
  g_test_add_data_func_full("/libmimircache/test_profilerLRU_more1_plain_str", reader, test_profilerLRU_more1, test_teardown);

  reader = setup_csv_reader_obj_num();
  g_test_add_data_func("/libmimircache/test_profilerLRU_basic_csv_num", reader, test_profilerLRU_basic);
  g_test_add_data_func_full("/libmimircache/test_profilerLRU_more1_csv_num", reader, test_profilerLRU_more1, test_teardown);

  reader = setup_csv_reader_obj_str();
  g_test_add_data_func("/libmimircache/test_profilerLRU_basic_csv_str", reader, test_profilerLRU_basic);
  g_test_add_data_func_full("/libmimircache/test_profilerLRU_more1_csv_str", reader, test_profilerLRU_more1, test_teardown);

  reader = setup_binary_reader();
  g_test_add_data_func("/libmimircache/test_profilerLRU_basic_binary", reader, test_profilerLRU_basic);
  g_test_add_data_func_full("/libmimircache/test_profilerLRU_more1_binary", reader, test_profilerLRU_more1, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func("/libmimircache/test_profilerLRU_basic_vscsi", reader, test_profilerLRU_basic);
  g_test_add_data_func_full("/libmimircache/test_profilerLRU_more1_vscsi", reader, test_profilerLRU_more1, test_teardown);

  return g_test_run();
}
