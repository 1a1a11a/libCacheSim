//
// Created by Juncheng Yang on 11/19/19.
//

#include "common.h"

// defined in reader.c file, not in public interface
int go_back_two_req(reader_t *const reader);

// TRUE DATA
size_t trace_length = 113872;
size_t trace_start_req_d[N_TEST_REQ] = {
    42932745, 42932746, 42932747, 40409911, 31954535, 6238199,
};
char *trace_start_req_s[N_TEST_REQ] = {
    "42932745", "42932746", "42932747", "40409911", "31954535", "6238199",
};
size_t trace_start_req_time[N_TEST_REQ] = {5633898368802, 5633898611441, 5633898745540,
                                           5633898967708, 5633899967748, 5633899967980};
size_t trace_start_req_size[N_TEST_REQ] = {512, 512, 512, 6656, 6144, 57344};
size_t trace_end_req_d = 42936150;
char *trace_end_req_s = "42936150";

void verify_req(reader_t *reader, request_t *req, int req_idx) {
  if (req_idx == -1) {
    if (obj_id_is_num(reader)) g_assert_true(req->obj_id == trace_end_req_d);
    return;
  }

  if (obj_id_is_num(reader)) g_assert_true(req->obj_id == trace_start_req_d[req_idx]);
  // we do not use g_quark_to_string() because it uses too much memory
  // else
  //   g_assert_cmpstr(g_quark_to_string(req->obj_id), ==,
  //                   trace_start_req_s[req_idx]);

  if (get_trace_type(reader) == CSV_TRACE || get_trace_type(reader) == BIN_TRACE ||
      get_trace_type(reader) == VSCSI_TRACE) {
    g_assert_true(req->clock_time == trace_start_req_time[req_idx] ||
                  req->clock_time == trace_start_req_time[req_idx] / 1000000);
    g_assert_true(req->obj_size == trace_start_req_size[req_idx]);
  }
}

void test_reader_basic(gconstpointer user_data) {
  reader_t *reader = (reader_t *)user_data;
  int i;
  request_t *req = new_request();

  g_assert_true(get_num_of_req(reader) == trace_length);

  // check reading
  for (i = 0; i < N_TEST_REQ; i++) {
    read_one_req(reader, req);
    verify_req(reader, req, i);
  }
  reset_reader(reader);

  // check resetting and reading
  for (i = 0; i < N_TEST_REQ; i++) {
    read_one_req(reader, req);
    verify_req(reader, req, i);
  }

  while (req->valid) {
    read_one_req(reader, req);
  }
  verify_req(reader, req, -1);
  reset_reader(reader);

  g_assert_true(get_num_of_req(reader) == trace_length);
  free_request(req);
}

void test_reader_more1(gconstpointer user_data) {
  reader_t *reader = (reader_t *)user_data;
  int i;
  request_t *req = new_request();

  // check skip_n_req
  g_assert_true(skip_n_req(reader, 4) == 4);
  for (i = 4; i < N_TEST_REQ; i++) {
    read_one_req(reader, req);
    verify_req(reader, req, i);
  }
  reset_reader(reader);

  // check reader rewind
  g_assert_true(skip_n_req(reader, 4) == 4);

  go_back_one_req(reader);
  read_one_req(reader, req);
  verify_req(reader, req, 3);

  go_back_two_req(reader);
  read_one_req(reader, req);
  verify_req(reader, req, 2);

  read_one_req_above(reader, req);
  verify_req(reader, req, 1);

  g_assert_true(get_num_of_req(reader) == trace_length);

  reader_set_read_pos(reader, 1.0);
  for (i = 0; i < trace_length; i++) {
    if (go_back_one_req(reader) != 0) break;
  }
  read_one_req(reader, req);
  verify_req(reader, req, 0);

  free_request(req);
}

void test_reader_more2(gconstpointer user_data) {
  reader_t *reader = (reader_t *)user_data;
  request_t *req = new_request();

  read_last_req(reader, req);
  verify_req(reader, req, -1);

  read_first_req(reader, req);
  verify_req(reader, req, 0);

  free_request(req);

  // check clone reader
  reader_t *cloned_reader = clone_reader(reader);
  test_reader_basic(cloned_reader);  // do not close cloned reader
  close_reader(cloned_reader);
}

void test_twr(gconstpointer user_data) {
  reader_t *reader = setup_reader("/Users/junchengy/twr.sbin", TWR_TRACE, NULL);
  gint64 n_req = get_num_of_req(reader);
  gint64 n_obj = 0;
  request_t *req = new_request();
  for (int i = 0; i < N_TEST_REQ; i++) {
    read_one_req(reader, req);
    //    printf("req %d: real time %lu, obj_id %llu, obj_size %ld, ttl %ld, op
    //    %d\n", i, (unsigned long) req->clock_time,
    //           (unsigned long long) req->obj_id, (long) req->obj_size,
    //           (long) req->ttl, req->op);
  }
  printf("%llu req %llu obj\n", (unsigned long long)n_req, (unsigned long long)n_obj);
}

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t *reader;

  reader = setup_plaintxt_reader_num();
  g_test_add_data_func("/libCacheSim/reader_basic_plain_num", reader, test_reader_basic);
  g_test_add_data_func("/libCacheSim/reader_more1_plain_num", reader, test_reader_more1);
  g_test_add_data_func_full("/libCacheSim/reader_more2_plain_num", reader, test_reader_more2, test_teardown);

  reader = setup_plaintxt_reader_str();
  g_test_add_data_func("/libCacheSim/reader_basic_plain_str", reader, test_reader_basic);
  g_test_add_data_func("/libCacheSim/reader_more1_plain_str", reader, test_reader_more1);
  g_test_add_data_func_full("/libCacheSim/reader_more2_plain_str", reader, test_reader_more2, test_teardown);

  reader = setup_csv_reader_obj_num();
  g_test_add_data_func("/libCacheSim/reader_basic_csv_num", reader, test_reader_basic);
  g_test_add_data_func("/libCacheSim/reader_more1_csv_num", reader, test_reader_more1);
  g_test_add_data_func_full("/libCacheSim/reader_more2_csv_num", reader, test_reader_more2, test_teardown);

  reader = setup_csv_reader_obj_str();
  g_test_add_data_func("/libCacheSim/reader_basic_csv_str", reader, test_reader_basic);
  g_test_add_data_func("/libCacheSim/reader_more1_csv_str", reader, test_reader_more1);
  g_test_add_data_func_full("/libCacheSim/reader_more2_csv_str", reader, test_reader_more2, test_teardown);

  reader = setup_binary_reader();
  g_test_add_data_func("/libCacheSim/reader_basic_binary", reader, test_reader_basic);
  g_test_add_data_func("/libCacheSim/reader_more1_binary", reader, test_reader_more1);
  g_test_add_data_func_full("/libCacheSim/reader_more2_binary", reader, test_reader_more2, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func("/libCacheSim/reader_basic_vscsi", reader, test_reader_basic);
  g_test_add_data_func("/libCacheSim/reader_more1_vscsi", reader, test_reader_more1);
  g_test_add_data_func_full("/libCacheSim/reader_more2_vscsi", reader, test_reader_more2, test_teardown);

  reader = setup_oracleGeneralBin_reader();
  g_test_add_data_func("/libCacheSim/reader_basic_oracleGeneral", reader, test_reader_basic);
  g_test_add_data_func("/libCacheSim/reader_more1_oracleGeneral", reader, test_reader_more1);
  g_test_add_data_func_full("/libCacheSim/reader_more2_oracleGeneral", reader, test_reader_more2, test_teardown);

  // g_test_add_data_func("/libCacheSim/test_twr", NULL, test_twr);
  return g_test_run();
}