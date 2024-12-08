//
// Created by Juncheng Yang on 11/24/19.
//

#include "common.h"

void test_distUtils_basic(gconstpointer user_data) {
  int32_t rd_true[N_TEST] = {-1, -1, -1, 7, -1, 86};
  // int32_t last_dist_true[N_TEST] = {-1, -1, -1, 7, -1, 137};
  int32_t last_dist_true[N_TEST] = {-1, -1, -1, 8, -1, 138};
  int32_t frd_true[N_TEST] = {11, 37, 49, -1, 8, -1};
  // int32_t next_dist_true[N_TEST] = {11, 59, 79, -1, 8, -1};
  int32_t next_dist_true[N_TEST] = {12, 60, 80, -1, 9, -1};
  int32_t* dist;
  int64_t array_size;

  reader_t* reader = (reader_t*)user_data;
  long i, j;

  dist = get_stack_dist(reader, STACK_DIST, &array_size);
  g_assert_cmpint(array_size, ==, get_num_of_req(reader));
  for (i = (long)get_num_of_req(reader) - 1, j = 0; j < N_TEST; i--, j++) {
    g_assert_cmpint(dist[i], ==, rd_true[j]);
  }

  dist = get_stack_dist(reader, FUTURE_STACK_DIST, &array_size);
  g_assert_cmpint(array_size, ==, get_num_of_req(reader));
  for (i = 6, j = 0; j < N_TEST; i++, j++) {
    g_assert_cmpint(dist[i], ==, frd_true[j]);
  }

  dist = get_access_dist(reader, DIST_SINCE_LAST_ACCESS, &array_size);
  g_assert_cmpint(array_size, ==, get_num_of_req(reader));
  for (i = (long)get_num_of_req(reader) - 1, j = 0; j < N_TEST; i--, j++) {
    g_assert_cmpint(dist[i], ==, last_dist_true[j]);
  }

  // dist = get_next_access_dist(reader);
  // for (i = 6, j = 0; j < N_TEST; i++, j++) {
  //   g_assert_cmpint(dist[i], ==, next_dist_true[j]);
  // }
}

void test_distUtils_more1(gconstpointer user_data) {
  int32_t rd_true[N_TEST] = {-1, -1, -1, 7, -1, 86};
  reader_t* reader = (reader_t*)user_data;
  int64_t array_size = 0;
  int32_t* rd = get_stack_dist(reader, STACK_DIST, &array_size);
  long i, j;
  for (i = (long)get_num_of_req(reader) - 1, j = 0; j < N_TEST; i--, j++) {
    g_assert_cmpint(rd[i], ==, rd_true[j]);
  }

  save_dist(reader, rd, array_size, "rd.save", STACK_DIST);
  g_free(rd);
  rd = load_dist(reader, "rd.save.STACK_DIST", &array_size);
  g_assert_cmpint(array_size, ==, get_num_of_req(reader));
  for (i = (long)get_num_of_req(reader) - 1, j = 0; j < N_TEST; i--, j++) {
    g_assert_cmpint(rd[i], ==, rd_true[j]);
  }
  g_free(rd);
}

int main(int argc, char* argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t* reader;

  reader = setup_plaintxt_reader_num();
  g_test_add_data_func("/libCacheSim/test_distUtils_basic_plain_num", reader, test_distUtils_basic);
  g_test_add_data_func_full("/libCacheSim/test_distUtils_more1_plain_num", reader, test_distUtils_more1, test_teardown);

  reader = setup_plaintxt_reader_str();
  g_test_add_data_func("/libCacheSim/test_distUtils_basic_plain_str", reader, test_distUtils_basic);
  g_test_add_data_func_full("/libCacheSim/test_distUtils_more1_plain_str", reader, test_distUtils_more1, test_teardown);

  reader = setup_csv_reader_obj_num();
  g_test_add_data_func("/libCacheSim/test_distUtils_basic_csv_num", reader, test_distUtils_basic);
  g_test_add_data_func_full("/libCacheSim/test_distUtils_more1_csv_num", reader, test_distUtils_more1, test_teardown);

  reader = setup_csv_reader_obj_str();
  g_test_add_data_func("/libCacheSim/test_distUtils_basic_csv_str", reader, test_distUtils_basic);
  g_test_add_data_func_full("/libCacheSim/test_distUtils_more1_csv_str", reader, test_distUtils_more1, test_teardown);

  reader = setup_binary_reader();
  g_test_add_data_func("/libCacheSim/test_distUtils_basic_binary", reader, test_distUtils_basic);
  g_test_add_data_func_full("/libCacheSim/test_distUtils_more1_binary", reader, test_distUtils_more1, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func("/libCacheSim/test_distUtils_basic_vscsi", reader, test_distUtils_basic);
  g_test_add_data_func_full("/libCacheSim/test_distUtils_more1_vscsi", reader, test_distUtils_more1, test_teardown);

  return g_test_run();
}
