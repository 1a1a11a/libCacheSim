#pragma once

#include "L2CacheInternal.h"
#include "const.h"
#include "learned.h"

#ifdef USE_XGBOOST
#include <xgboost/c_api.h>
#define safe_call(call)                                                                        \
  {                                                                                            \
    int err = (call);                                                                          \
    if (err != 0) {                                                                            \
      fprintf(stderr, "%s:%d: error in %s: %s\n", __FILE__, __LINE__, #call,                   \
              XGBGetLastError());                                                              \
      exit(1);                                                                                 \
    }                                                                                          \
  }
#elif defined(USE_GBM)
#include <LightGBM/c_api.h>
#define safe_call(call)                                                                        \
  {                                                                                            \
    int err = (call);                                                                          \
    if (err != 0) {                                                                            \
      fprintf(stderr, "%s:%d: error in %s: %s\n", __FILE__, __LINE__, #call, LastErrorMsg());  \
      exit(1);                                                                                 \
    }                                                                                          \
  }
#endif

bool prepare_one_row(cache_t *cache, segment_t *curr_seg, bool training_data, feature_t *x,
                     train_y_t *y);