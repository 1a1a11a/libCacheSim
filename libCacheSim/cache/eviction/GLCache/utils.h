#pragma once

#include "GLCacheInternal.h"

#define MAYBE_RESIZE_MATRIX(matrix_type, matrix_p, expect_dim1, expect_dim2, curr_dim1_p,      \
                            curr_dim2_p)                                                       \
  do {                                                                                         \
    if ((matrix_p) == NULL || (expect_dim1) > (*(curr_dim1_p))                                 \
        || (expect_dim2) > (*(curr_dim2_p))) {                                                 \
      my_free(sizeof(matrix_type) * (*(curr_dim1_p)) * (*(curr_dim2_p)), (matrix_p));          \
      matrix_p = (matrix_type *) malloc(sizeof(matrix_type) * (expect_dim1) * (expect_dim2));  \
      *(curr_dim1_p) = (expect_dim1);                                                          \
      *(curr_dim2_p) = (expect_dim2);                                                          \
    }                                                                                          \
  } while (0)

#define MAYBE_RESIZE_VECTOR(matrix_type, matrix_p, expect_dim, curr_dim_p)                     \
  do {                                                                                         \
    if ((matrix_p) == NULL || (expect_dim) > (*(curr_dim_p))) {                                \
      my_free(sizeof(matrix_type) * (*(curr_dim_p)), (matrix_p));                              \
      matrix_p = (matrix_type *) malloc(sizeof(matrix_type) * (expect_dim));                   \
      *(curr_dim_p) = expect_dim;                                                              \
    }                                                                                          \
  } while (0)

static inline int count_hash_chain_len(cache_obj_t *cache_obj) {
  int n = 0;
  while (cache_obj) {
    n += 1;
    cache_obj = cache_obj->hash_next;
  }
  return n;
}

static inline int cmp_double(const void *p1, const void *p2) {
  if (*(double *) p1 < *(double *) p2) return -1;
  else
    return 1;
}

static int find_argmin_float(const float *y, int n_elem) {
  float min_val = (float) INT64_MAX;
  int min_val_pos = -1;
  for (int i = 0; i < n_elem; i++) {
    if (y[i] < min_val) {
      min_val = y[i];
      min_val_pos = i;
    }
  }
  DEBUG_ASSERT(min_val_pos != -1);
  return min_val_pos;
}

static int find_argmin_double(const double *y, int n_elem) {
  double min_val = (double) INT64_MAX;
  int min_val_pos = -1;
  for (int i = 0; i < n_elem; i++) {
    if (y[i] < min_val) {
      min_val = y[i];
      min_val_pos = i;
    }
  }
  DEBUG_ASSERT(min_val_pos != -1);
  return min_val_pos;
}

static inline uint64_t gettime_usec() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return tv.tv_sec * 1000000 + tv.tv_usec;
}

static int cmp_double_double_pair(const void *p1, const void *p2) {
  dd_pair_t *pair1 = (dd_pair_t *) p1;
  dd_pair_t *pair2 = (dd_pair_t *) p2;
  if (pair1->x < pair2->x * 0.999) return -1;
  else if (pair1->x > pair2->x * 1.001)
    return 1;
  else {
    if (pair1->y < pair2->y * 0.999) return -1;
    else if (pair1->y > pair2->y * 1.001)
      return 1;
    else
      return 0;
  }
  return 0;
}

