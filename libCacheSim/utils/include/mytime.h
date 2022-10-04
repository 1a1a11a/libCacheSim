#pragma once

#include <sys/time.h>

#define TIMEVAL_TO_USEC(tv) ((long long)((tv).tv_sec * 1000000 + (tv).tv_usec))
#define TIMEVAL_TO_SEC(tv) ((double)((tv).tv_sec + (tv).tv_usec / 1000000.0))

static inline double time_diff(struct timeval t0, struct timeval t1) {
  return (double)(t1.tv_sec - t0.tv_sec) +
         (double)(t1.tv_usec - t0.tv_usec) / 1000000;
}
