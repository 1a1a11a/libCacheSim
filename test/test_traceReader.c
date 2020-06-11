//
// Created by Juncheng Yang on 11/19/19.
//

#include "test_common.h"

void init_test() {
  g_assert_cmpint(1, <=, 2);
}

void mytest2() {
  g_assert_cmpint(1, >, 2);
}


// TRUE DATA
size_t trace_length = 113872;
size_t trace_start_req_d[N_TEST_REQ] = {42932745, 42932746, 42932747, 40409911, 31954535, 6238199,};
//size_t trace_start_req_s[N_TEST_REQ] = {1, 2, 3, 4, 5, 6,};
char *trace_start_req_s[N_TEST_REQ] = {"42932745", "42932746", "42932747", "40409911", "31954535", "6238199",};
size_t trace_start_req_time[N_TEST_REQ] = {5633898368802, 5633898611441, 5633898745540, 5633898967708, 5633899967748,
                                           5633899967980};
size_t trace_start_req_size[N_TEST_REQ] = {512, 512, 512, 6656, 6144, 57344};
size_t trace_end_req_d = 42936150;
//size_t trace_end_req_s = 1;
char *trace_end_req_s = "42936150";


void verify_req(reader_t *reader, request_t *req, int req_idx) {
//  printf("%lu - %lu\n", GPOINTER_TO_SIZE (req->obj_id_ptr), trace_start_req_d[req_idx]);
  if (reader->base->obj_id_type == OBJ_ID_NUM)
    g_assert_true(req->obj_id_int == trace_start_req_d[req_idx]);
  else if (reader->base->obj_id_type == OBJ_ID_STR)
    g_assert_cmpstr(g_quark_to_string(req->obj_id_int), ==, trace_start_req_s[req_idx]);

  if (reader->base->trace_type == CSV_TRACE || reader->base->trace_type == BIN_TRACE ||
      reader->base->trace_type == VSCSI_TRACE) {
    g_assert_true(req->real_time == trace_start_req_time[req_idx] || req->real_time == trace_start_req_time[req_idx]/1000000);
    g_assert_true(req->obj_size == trace_start_req_size[req_idx]);
  }
}

void test_reader_basic(gconstpointer user_data) {
  reader_t *reader = (reader_t *) user_data;
  gint i;
  request_t *req = new_request();
  // check length
  g_assert_true(get_num_of_req(reader) == trace_length);

  // check reading
  for (i = 0; i < N_TEST_REQ; i++) {
    read_one_req(reader, req);
    verify_req(reader, req, i);
  }
  reset_reader(reader);

  // check reseting and reading
  for (i = 0; i < N_TEST_REQ; i++) {
    read_one_req(reader, req);
    verify_req(reader, req, i);
  }

  size_t last_read_num = 0;
  const gchar *last_read_str = NULL;
  // check reading full data
  while (req->valid) {
    if (reader->base->obj_id_type == OBJ_ID_NUM)
      last_read_num = req->obj_id_int;
    else if (reader->base->obj_id_type == OBJ_ID_STR)
      last_read_str = g_quark_to_string(req->obj_id_int);
    else
      g_assert_not_reached();

    read_one_req(reader, req);
  }
  if (reader->base->obj_id_type == OBJ_ID_NUM)
    g_assert_true(last_read_num == trace_end_req_d);
  else if (reader->base->obj_id_type == OBJ_ID_STR)
    g_assert_cmpstr(last_read_str, ==, trace_end_req_s);
  reset_reader(reader);

  g_assert_true(get_num_of_req(reader) == trace_length);
  free_request(req);
}


void test_reader_more1(gconstpointer user_data) {
  reader_t *reader = (reader_t *) user_data;
  int i;
  request_t *req = new_request();

  // check skip_N_elements
  g_assert_true(skip_N_elements(reader, 4) == 4);
  for (i = 4; i < N_TEST_REQ; i++) {
    read_one_req(reader, req);
    if (reader->base->obj_id_type == OBJ_ID_NUM)
      g_assert_true(GPOINTER_TO_SIZE(req->obj_id_int) == trace_start_req_d[i]);
    else if (reader->base->obj_id_type == OBJ_ID_STR)
      g_assert_cmpstr(g_quark_to_string(req->obj_id_int), ==, trace_start_req_s[i]);

    if (reader->base->trace_type == CSV_TRACE || reader->base->trace_type == BIN_TRACE ||
        reader->base->trace_type == VSCSI_TRACE) {
      g_assert_true(req->real_time == trace_start_req_time[i] || req->real_time == trace_start_req_time[i]/1000000);
      g_assert_true(req->obj_size == trace_start_req_size[i]);
    }
  }
  reset_reader(reader);

  // check reader rewind
  g_assert_true(skip_N_elements(reader, 4) == 4);

  go_back_one_line(reader);
  read_one_req(reader, req);
  verify_req(reader, req, 3);

  go_back_two_lines(reader);
  read_one_req(reader, req);
  verify_req(reader, req, 2);

  read_one_req_above(reader, req);
  verify_req(reader, req, 1);

  g_assert_true(get_num_of_req(reader) == trace_length);
  free_request(req);
}

void test_reader_more2(gconstpointer user_data) {
  reader_t *reader = (reader_t *) user_data;

  // check clone reader
  reader_t *cloned_reader = clone_reader(reader);
  test_reader_basic(cloned_reader); // do not close cloned reader
  close_reader(cloned_reader);
}


void test_twr(gconstpointer user_data) {
  reader_t *reader = setup_reader("/Users/junchengy/twr.sbin", TWR_TRACE, OBJ_ID_NUM, NULL);
  gint64 n_req = get_num_of_req(reader);
  gint64 n_obj = 0;
  request_t *req = new_request();
  for (int i = 0; i < N_TEST_REQ; i++) {
    read_one_req(reader, req);
//    printf("req %d: real time %lu, obj_id %llu, obj_size %ld, ttl %ld, op %d\n", i, (unsigned long) req->real_time,
//           (unsigned long long) req->obj_id_int, (long) req->obj_size, (long) req->ttl, req->op);
  }
  printf("%llu req %llu obj\n", (unsigned long long) n_req, (unsigned long long) n_obj);
}


int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t *reader;

  g_test_add_func("/libmimircache/test1", init_test);

  reader = setup_plaintxt_reader_num();
  g_test_add_data_func("/libmimircache/test_reader_basic_plain_num", reader, test_reader_basic);
  g_test_add_data_func("/libmimircache/test_reader_more1_plain_num", reader, test_reader_more1);
  g_test_add_data_func_full("/libmimircache/test_reader_more2_plain_num", reader, test_reader_more2, test_teardown);

  reader = setup_plaintxt_reader_str();
  g_test_add_data_func("/libmimircache/test_reader_basic_plain_str", reader, test_reader_basic);
  g_test_add_data_func("/libmimircache/test_reader_more1_plain_str", reader, test_reader_more1);
  g_test_add_data_func_full("/libmimircache/test_reader_more2_plain_str", reader, test_reader_more2, test_teardown);

  reader = setup_csv_reader_obj_num();
  g_test_add_data_func("/libmimircache/test_reader_basic_csv_num", reader, test_reader_basic);
  g_test_add_data_func("/libmimircache/test_reader_more1_csv_num", reader, test_reader_more1);
  g_test_add_data_func_full("/libmimircache/test_reader_more2_csv_num", reader, test_reader_more2, test_teardown);

  reader = setup_csv_reader_obj_str();
  g_test_add_data_func("/libmimircache/test_reader_basic_csv_str", reader, test_reader_basic);
  g_test_add_data_func("/libmimircache/test_reader_more1_csv_str", reader, test_reader_more1);
  g_test_add_data_func_full("/libmimircache/test_reader_more2_csv_str", reader, test_reader_more2, test_teardown);

  reader = setup_binary_reader();
  g_test_add_data_func("/libmimircache/test_reader_basic_binary", reader, test_reader_basic);
  g_test_add_data_func("/libmimircache/test_reader_more1_binary", reader, test_reader_more1);
  g_test_add_data_func_full("/libmimircache/test_reader_more2_binary", reader, test_reader_more2, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func("/libmimircache/test_reader_basic_vscsi", reader, test_reader_basic);
  g_test_add_data_func("/libmimircache/test_reader_more1_vscsi", reader, test_reader_more1);
  g_test_add_data_func_full("/libmimircache/test_reader_more2_vscsi", reader, test_reader_more2, test_teardown);

  // g_test_add_data_func("/libmimircache/test_twr", NULL, test_twr);
  return g_test_run();
}