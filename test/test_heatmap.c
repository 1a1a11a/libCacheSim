//
// Created by Juncheng Yang on 5/7/20.
//


#include "common.h"
#include "../libCacheSim/include/libCacheSim/experimental/distHeatmap.h"
#include "../libCacheSim/include/libCacheSim/experimental/heatmap.h"


void test_heatmap_basic(gconstpointer user_data){
  gint64 rd_true[N_TEST] = {-1, -1, -1, 7, -1, 86};
  gint64 last_dist_true[N_TEST] = {-1, -1, -1, 8, -1, 138};
  gint64 frd_true[N_TEST] = {11, 37, 49, -1, 8, -1};
  gint64 next_dist_true[N_TEST] = {12, 60, 80, -1, 9, -1};
  gint64 *dist;

  reader_t* reader = (reader_t*) user_data;
  long i, j;

  heatmap_plot_matrix_t* hm_matrix = get_reuse_time_heatmap_matrix(reader, 300*1000000, 1.2);
//  print_heatmap_plot_matrix(hm_matrix);
  free_heatmap_plot_matrix(hm_matrix);
}




int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  reader_t *reader;

  reader = setup_csv_reader_obj_num();
  g_test_add_data_func("/libCacheSim/test_distUtils_basic_csv_num", reader, test_heatmap_basic);
//  g_test_add_data_func_full("/libCacheSim/test_distUtils_more1_csv_num", reader, test_distUtils_more1, test_teardown);
  reader = setup_csv_reader_obj_str();
  g_test_add_data_func("/libCacheSim/test_distUtils_basic_csv_str", reader, test_heatmap_basic);
//  g_test_add_data_func_full("/libCacheSim/test_distUtils_more1_csv_str", reader, test_distUtils_more1, test_teardown);

  reader = setup_binary_reader();
  g_test_add_data_func("/libCacheSim/test_distUtils_basic_binary", reader, test_heatmap_basic);
//  g_test_add_data_func_full("/libCacheSim/test_distUtils_more1_binary", reader, test_distUtils_more1, test_teardown);

  reader = setup_vscsi_reader();
  g_test_add_data_func("/libCacheSim/test_distUtils_basic_vscsi", reader, test_heatmap_basic);
//  g_test_add_data_func_full("/libCacheSim/test_distUtils_more1_vscsi", reader, test_distUtils_more1, test_teardown);

  return g_test_run();
}







