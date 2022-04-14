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

static inline void resize_matrix(L2Cache_params_t *params, feature_t **x_p, pred_t **y_p,
                                 int32_t *size_p, int64_t new_size) {
  learner_t *learner = &params->learner;
  if ((*size_p) != 0) {
    // free previously allocated memory
    // TODO: use realloc instead of free
    DEBUG_ASSERT(*x_p != NULL);
    my_free(sizeof(feature_t) * (*size_p) * learner->n_feature, *x_p);
    my_free(sizeof(pred_t) * (*size_p), *y_p);
  }

  *x_p = my_malloc_n(feature_t, new_size * learner->n_feature);
  memset(*x_p, 0, sizeof(feature_t) * new_size * learner->n_feature);
  *y_p = my_malloc_n(pred_t, new_size);
  memset(*y_p, 0, sizeof(pred_t) * new_size);
  *size_p = new_size;
}
