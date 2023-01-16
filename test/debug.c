//
// Created by Juncheng Yang on 4/23/20.
//

#include <assert.h>
#include <glib.h>
#include <gmodule.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// #include "../libCacheSim/include/libCacheSim.h"
// #include "../libCacheSim/utils/include/mymath.h"

// #include "common.h"

// #define TIMEVAL_TO_USEC(tv) ((long long) (tv.tv_sec*1000000+tv.tv_usec))
// #define TIMEVAL_TO_SEC(tv) ((double) (tv.tv_sec+tv.tv_usec/1000000.0))
// //#define my_malloc(type) (type*) malloc(sizeof(type))
// #define my_malloc0(type) malloc(sizeof(type))

// static void print_rusage_diff(struct rusage r1, struct rusage r2) {
//   printf("******  CPU user time %.2lf s, sys time %.2lf s\n",
//          (TIMEVAL_TO_SEC(r2.ru_utime) - TIMEVAL_TO_SEC(r1.ru_utime)),
//          (TIMEVAL_TO_SEC(r2.ru_stime) - TIMEVAL_TO_SEC(r1.ru_stime)));

//   printf(
//       "******  Mem RSS %.2lf MB, soft page fault %ld - hard page fault %ld,
//       voluntary context switches %ld - involuntary %ld\n", (double)
//       (r2.ru_maxrss - r1.ru_maxrss) / (1024.0), (r2.ru_minflt -
//       r1.ru_minflt), (r2.ru_majflt - r1.ru_majflt), (r2.ru_nvcsw -
//       r1.ru_nvcsw), (r2.ru_nivcsw - r1.ru_nivcsw));
// }

// static void f1(int argc, char* argv[]) {
//   int n_threads = 1;
//   char *trace_path = "/Users/junchengy/twr.sbin";
// //  if (argc >= 2)
// //    trace_path = argv[1];
// //  if (argc >= 3)
// //    n_threads = atoi(argv[2]);

// //  reader_init_param_t init_params = {.binary_fmt_str="III", .time_field=1,
// .obj_id_field=2, .obj_size_field=3};
// //  reader_t *reader = setup_reader(trace_path, TWR_BIN_TRACE, OBJ_ID_NUM,
// NULL);
// //  reader_t *reader = setup_reader("../../data/trace.vscsi", VSCSI_TRACE,
// OBJ_ID_NUM, NULL);
//   reader_t *reader = setup_reader("../../data/trace.vscsi", VSCSI_TRACE,
//   OBJ_ID_NUM, NULL);

//   common_cache_params_t cc_params = {.cache_size=1*1024*1024,
//   .default_ttl=300*86400};
// //  cache_t *cache = LFU_init(cc_params, NULL);
//   cache_t *cache = LHD_init(cc_params, NULL);
//   gint num_of_sizes = 1;
// //  guint64 cache_sizes[] = {2*MB, 8*MB, 16*MB, 32*MB, 64*MB, 128*MB, 8*GB,
// 16*GB};
// //  guint64 cache_sizes[] = {1*GB};
// //  guint64 cache_sizes[] = {128 * MiB};

//   n_threads = num_of_sizes = 4;
//   guint64 cache_sizes[] = {268435456, 268435456, 268435456, 268435456};

//   cache_stat_t *res = simulate_at_multi_sizes(reader, cache, num_of_sizes,
//   cache_sizes, NULL, 0, n_threads);

//   for (int i=0; i<num_of_sizes; i++){
//     printf("%s cache size %lld req %lld miss %lld req_byte %lld miss_byte
//     %lld\n", __func__,
//            (long long) res[i].cache_size, (long long) res[i].n_req, (long
//            long) res[i].n_miss, (long long) res[i].n_req_byte, (long long)
//            res[i].n_miss_byte);
//   }
//   cache->cache_free(cache);
//   my_free(sizeof(cache_stat_t), res);
//   close_reader(reader);
// }

// static void f2(int argc, char* argv[]){
//   reader_t *reader = setup_reader("data/syn3.txt", PLAIN_TXT_TRACE,
//   OBJ_ID_NUM, NULL); common_cache_params_t cc_params = {.cache_size = 2};
//   cache_t *cache = MRU_init(cc_params, NULL);

//   request_t *req = new_request();
//   read_one_req(reader, req);
//   while (req->valid) {
//     bool ck = cache->get(cache, req);
//     printf("req %ld hit %d\n", (long) req->obj_id, ck == cache_ck_hit);
//     read_one_req(reader, req);
//   }
// }

// static void f3(int argc, char* argv[]) {
//   int n_threads = 1;
//   char *trace_path = NULL;
//   if (argc >= 2)
//     trace_path = argv[1];
//   if (argc >= 3)
//     n_threads = atoi(argv[2]);

//   reader_t *reader = setup_reader("data/trace.vscsi", VSCSI_TRACE,
//   OBJ_ID_NUM, NULL);
// //  reader_t *reader = setup_reader("/Users/junchengy/akamai.bin", BIN_TRACE,
// OBJ_ID_NUM, &init_params);

//   common_cache_params_t cc_params = {.cache_size=1024*1024*1024,
//   .default_ttl=0};
// //  cache_t *cache = create_cache("LRU", cc_params, NULL);
//   cache_t *cache = create_test_cache("LFUDA", cc_params, reader, NULL);

//   request_t *req = new_request();
//   read_one_req(reader, req);

//   uint64_t req_cnt = 0, miss_cnt = 0;

//   while (req->valid) {
//     req_cnt++;
//     if (cache->get(cache, req) != cache_ck_hit) {
//       miss_cnt++;
//     }
//     read_one_req(reader, req);
// //    req->obj_size = 1;
//   }

//   printf("%" PRIu64 "/%"PRIu64"\n", miss_cnt, req_cnt);
//   cache->cache_free(cache);
//   close_reader(reader);
// }

// typedef struct {
//   uint64_t t[8];
// } item;

int main(int argc, char *argv[]) {
  // f1(argc, argv);
  //  f2(argc, argv);
  //  f3(argc, argv);
  return 0;
}
