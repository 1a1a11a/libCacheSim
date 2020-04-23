//
// measured the performance of cache eviction algorithm
//
// Created by Juncheng Yang on 4/23/20.
//

#ifndef LIBMIMIRCACHE_CACHEALGOPERF_H
#define LIBMIMIRCACHE_CACHEALGOPERF_H


#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>
#include <sys/time.h>
#include <glib.h>
#include "../../include/mimircache/cache.h"


static inline double time_diff(struct timeval t0, struct timeval t1) {
  return (double) (t1.tv_sec - t0.tv_sec) + (double) (t1.tv_usec - t0.tv_usec) / 1000000;
}

static inline double time_since(struct timeval t0) {
  struct timeval t1;
  gettimeofday(&t1, 0);
  return time_diff(t0, t1);
}

gint64 measure_qps(cache_t* cache, double expected_miss_ratio);



#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_CACHEALGOPERF_H
