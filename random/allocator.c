//
// Created by Juncheng Yang on 6/9/20.
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


#define TIMEVAL_TO_USEC(tv) ((long long) (tv.tv_sec*1000000+tv.tv_usec))
#define TIMEVAL_TO_SEC(tv) ((double) (tv.tv_sec+tv.tv_usec/1000000.0))
#define my_malloc(type) (type*) malloc(sizeof(type))


typedef void *(*allocator)(size_t);

typedef void (*free_func)(void *);

typedef void (*workload_func)(allocator, const char *);

#define N (1024L * 1024 * 100)
#define ALIGN_SIZE 64
static int alloc_size = 0;
static int alloc_type = 0;
static int workload_idx = 0;
static void *it[N];



static inline void g_slice_free2(void *mem) {
  g_slice_free1(alloc_size, mem);
}

static inline void* aligned_alloc2(size_t size){
  return aligned_alloc(ALIGN_SIZE, size);
}

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

void workload0(allocator alloc, const char *allocator_name) {
  /* single size allocation */
  printf("%s alloc size %d, expected RSS %ld MiB\n", allocator_name, alloc_size,
         alloc_size * (N / 1024 / 1024L));

  for (int i = 0; i < N; i++) {
    it[i] = alloc(alloc_size);
    *(uint64_t*) (it[i]) = i;
  }
}


void workload1(allocator alloc, const char *allocator_name) {
  /* single size allocation */
  printf("%s alloc size %d, expected RSS %ld MiB\n", allocator_name, alloc_size,
         alloc_size * (N / 1024 / 1024L));
  for (int i = 0; i < N; i++) {
    it[i] = alloc(alloc_size);
  }
}

void eval_alloc_perf() {
  const char *allocator_names[] = {"malloc", "g_malloc", "g_slice_alloc", "my_malloc"};
  const allocator allocators[] = {malloc, g_malloc, g_slice_alloc, aligned_alloc2};
  const free_func free_funcs[] = {free, g_free, g_slice_free2, free};
  const workload_func workloads[] = {workload0, workload1};


  struct rusage r_usage_before, r_usage_after;
  getrusage(RUSAGE_SELF, &r_usage_before);

  workloads[workload_idx](allocators[alloc_type], allocator_names[alloc_type]);

//  for (int i = 0; i < N; i++)
//    free_funcs[alloc_type](it[i]);

  getrusage(RUSAGE_SELF, &r_usage_after);
  print_rusage_diff(r_usage_before, r_usage_after);


//  sleep(20);
}


/**
 * LD_PRELOAD=/home/jason/software/source/gperftools-2.7/.libs/libtcmalloc.so
 * LD_PRELOAD=/usr/local/lib/libjemalloc.so
 * LD_PRELOAD=/usr/lib/libhoard.so
 * gcc allocator.c $(pkg-config --cflags --libs glib-2.0) -o binAllocatorEval;
 * gcc allocator.c -ltcmalloc $(pkg-config --cflags --libs glib-2.0) -o binAllocatorEval;
 * gcc allocator.c -ljemalloc $(pkg-config --cflags --libs glib-2.0) -o binAllocatorEval;
 * gcc allocator.c -lhoard $(pkg-config --cflags --libs glib-2.0) -o binAllocatorEval; 
 * gcc allocator.c -L/home/jason/software/source/ptmalloc/ptmalloc.o $(pkg-config --cflags --libs glib-2.0) -o binAllocatorEval; 
 * 
 * 
 * for s in 4 8 16 32 64 128 256; do
 *   ./binAllocatorEval 0 $s 0
 * done 
 */

int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf("usage %s alloc_type alloc_size workload\n", argv[0]);
    exit(1);
  }
  alloc_type = atoi(argv[1]);
  alloc_size = atoi(argv[2]);
  workload_idx = atoi(argv[3]);
  eval_alloc_perf();
  return 0;
}

