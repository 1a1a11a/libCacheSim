//
// Created by Juncheng Yang on 11/21/19.
//

#include "common.h"

void test_profilerLRU_basic(gconstpointer user_data) {
  // note LRU profiler does not support variable size request
  reader_t *reader = (reader_t *)user_data;
  long i;
  double *mr;

  double mr_last_size20_true = 1 - 0.072985;
  //  double mr_last_true = 1 - 0.569921;
  //  guint64 hc_true[N_TEST] = {0, 2685, 3347, 3908, 4666, 4904};

  double omr_true[N_TEST] = {1, 0.976421, 0.970607, 0.965681, 0.959024, 0.956934};
  //  double bmr_true[N_TEST] = {};

  mr = get_lru_obj_miss_ratio(reader, get_num_of_req(reader));
  for (i = 0; i < N_TEST; i++) {
    g_assert_cmpfloat(fabs(mr[i] - omr_true[i]), <=, 0.0001);
  }
  g_free(mr);

  mr = get_lru_obj_miss_ratio(reader, 20);
  g_assert_cmpfloat(fabs(mr[20] - mr_last_size20_true), <=, 0.0001);
  g_free(mr);
}

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t *reader;

  reader = setup_plaintxt_reader_num();
  g_test_add_data_func("/libCacheSim/test_profilerLRU_basic_plain_num", reader, test_profilerLRU_basic);

  reader = setup_plaintxt_reader_str();
  g_test_add_data_func("/libCacheSim/test_profilerLRU_basic_plain_str", reader, test_profilerLRU_basic);

  reader = setup_csv_reader_obj_num();
  g_test_add_data_func("/libCacheSim/test_profilerLRU_basic_csv_num", reader, test_profilerLRU_basic);

  reader = setup_csv_reader_obj_str();
  g_test_add_data_func("/libCacheSim/test_profilerLRU_basic_csv_str", reader, test_profilerLRU_basic);

  reader = setup_binary_reader();
  g_test_add_data_func("/libCacheSim/test_profilerLRU_basic_binary", reader, test_profilerLRU_basic);

  reader = setup_vscsi_reader();
  g_test_add_data_func("/libCacheSim/test_profilerLRU_basic_vscsi", reader, test_profilerLRU_basic);

  return g_test_run();
}
