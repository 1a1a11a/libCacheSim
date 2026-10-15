#pragma once

#include "libCacheSim/reader.h"

/* Private reader state. Position counts source requests, before sampling. */
typedef struct {
  uint64_t position;
  uint64_t n_objects;
  uint64_t seed;
  double alpha;
  double h_integral_max;
  double h_integral_min;
  double squeeze;
} synthetic_params_t;

int synthetic_setup(reader_t *reader);
bool synthetic_has_next(const reader_t *reader);
int synthetic_read_one_req(reader_t *reader, request_t *req);
