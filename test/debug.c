//
// Created by Juncheng Yang on 4/23/20.
//


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <glib.h>
#include <gmodule.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>

//#include "test_common.h"
//#include "../libMimircache/include/mimircache/distHeatmap.h"

#define TIMEVAL_TO_USEC(tv) ((long long) (tv.tv_sec*1000000+tv.tv_usec))
#define TIMEVAL_TO_SEC(tv) ((double) (tv.tv_sec+tv.tv_usec/1000000.0))
#define my_malloc(type) (type*) malloc(sizeof(type))
#define my_malloc0(type) malloc(sizeof(type))


void print_rusage_diff(struct rusage r1, struct rusage r2) {
  printf("******  CPU user time %.2lf s, sys time %.2lf s\n",
         (TIMEVAL_TO_SEC(r2.ru_utime) - TIMEVAL_TO_SEC(r1.ru_utime)),
         (TIMEVAL_TO_SEC(r2.ru_stime) - TIMEVAL_TO_SEC(r1.ru_stime)));

  printf(
      "******  Mem RSS %.2lf MB, soft page fault %ld - hard page fault %ld, voluntary context switches %ld - involuntary %ld\n",
      (double) (r2.ru_maxrss - r1.ru_maxrss) / (1024.0),
      (r2.ru_minflt - r1.ru_minflt), (r2.ru_majflt - r1.ru_majflt),
      (r2.ru_nvcsw - r1.ru_nvcsw), (r2.ru_nivcsw - r1.ru_nivcsw));
}

/*
void f1(int argc, char* argv[]) {
  int n_threads = 1;
  char *trace_path = "/Users/junchengy/twr.sbin";
//  char *trace_path = "/home/jason//data/twr/warmupEvalSplit/simclusters_v2_entity_cluster_scores_cache.0.eval.sbin";
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
  common_cache_params_t cc_params = {.cache_size=1024*1024*1024, .default_ttl=2};
  slab_init_params_t slab_init_params = {.slab_size=1024*1024, .per_obj_metadata_size=0, .slab_move_strategy=recency_t};
  cache_t *cache = create_cache("TTL_FIFO", cc_params, NULL);
//  cache_t *cache = create_cache("slabObjLRU", cc_params, NULL);
//  cache_t *cache = create_cache("slabObjLRU", cc_params, &slab_init_params);
//  profiler_res_t *res = get_miss_ratio_curve_with_step_size(reader, cache, STEP_SIZE, 0, 4);
  gint num_of_sizes = 6;
//  guint64 cache_sizes[] = {2*MB, 8*MB, 16*MB, 32*MB, 64*MB, 128*MB, 8*GB, 16*GB};
  guint64 cache_sizes[] = {16*MB, 64*MB, 256*MB, 1*GB, 4*GB, 16*GB};
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
*/


typedef struct {
  uint64_t t[8];
} item;


int main(int argc, char *argv[]) {
//  f1(argc, argv);
//  f3(atoi(argv[1]), atoi(argv[2]));
  return 0;
}
