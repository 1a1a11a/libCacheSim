//
// Created by Juncheng Yang on 4/23/20.
//



#include "test_common.h"
#include "../libMimircache/include/mimircache/distHeatmap.h"


void f1(int argc, char* argv[]) {
  int n_threads = 4;
  char *trace_path = "/Users/junchengy/twr.sbin";
  if (argc >= 2)
    trace_path = argv[1];
  if (argc >= 3)
    n_threads = atoi(argv[2]);

//  printf("trace %s %d threads\n", trace_path, n_threads);
  reader_init_param_t init_params = {.binary_fmt="III", .real_time_field=1, .obj_id_field=2, .obj_size_field=3};
  reader_t *reader = setup_reader(trace_path, TWR_TRACE, OBJ_ID_NUM, NULL);
//  reader_t *reader = setup_reader("../../data/trace.vscsi", VSCSI_TRACE, OBJ_ID_NUM, NULL);
//  reader_t *reader = setup_reader("/Users/junchengy/akamai.bin", BIN_TRACE, OBJ_ID_NUM, &init_params);

//  reader_t* reader = setup_reader("/Users/junchengy/test", PLAIN_TXT_TRACE, OBJ_ID_NUM, NULL);
  common_cache_params_t cc_params = {.cache_size=1024*1024*1024, .obj_id_type=reader->base->obj_id_type, .default_ttl=2};
//  cache_t *cache = create_cache("FIFO", cc_params, NULL);
  cache_t *cache = create_cache("slabLRC", cc_params, NULL);
//  profiler_res_t *res = get_miss_ratio_curve_with_step_size(reader, cache, STEP_SIZE, 0, 4);
  gint num_of_sizes = 8;
  guint64 cache_sizes[] = {2*MB, 8*MB, 16*MB, 32*MB, 64*MB, 128*MB, 8*GB, 16*GB};
//  guint64 cache_sizes[] = {16*MB, 32*MB, 64*MB, 128*MB, 8*GB, 16*GB};
//  guint64 cache_sizes[] = {1*GB, 4*GB, 8*GB, 16*GB};
  profiler_res_t *res = get_miss_ratio_curve(reader, cache, num_of_sizes, cache_sizes, NULL, 0, n_threads);

  for (int i=0; i<num_of_sizes; i++){
    printf("%s cache size %lld req %lld miss %lld req_byte %lld miss_byte %lld\n", __func__,
           (long long) res[i].cache_size, (long long) res[i].req_cnt, (long long) res[i].miss_cnt, (long long) res[i].req_byte, (long long) res[i].miss_byte);
  }
  cache->core.cache_free(cache);
  g_free(res);
  close_reader(reader);
}

void f2(int argc, char* argv[]){
  reader_t *reader = setup_reader(argv[1], TWR_TRACE, OBJ_ID_NUM, NULL);
  get_last_access_dist_heatmap_matrix(reader, 300, 1.2);
}


int main(int argc, char* argv[]){
  f1(argc, argv);
  return 0;
}