//
// Created by Juncheng Yang on 3/19/20.
//

#ifndef LIBMIMIRCACHE_PROFILERINTERNAL_H
#define LIBMIMIRCACHE_PROFILERINTERNAL_H


#ifdef __cplusplus
extern "C"
{
#endif


#include "../../include/mimircache.h"


typedef struct profiler_multithreading_params {
  reader_t *reader;
  cache_t *cache;
  guint64 n_warmup_req;   // num of requests used for warming up cache
  profiler_res_t *result;
//  guint64 bin_size;
  GMutex mtx;             // prevent simultaneous write to progress
  guint64 *progress;
  gpointer other_data;
} prof_mt_params_t;


#ifdef __cplusplus
}
#endif



#endif //LIBMIMIRCACHE_PROFILERINTERNAL_H
