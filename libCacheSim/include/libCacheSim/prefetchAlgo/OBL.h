#ifndef OBL_H
#define OBL_H
#include <assert.h>
#include <glib.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "../cache.h"

typedef struct OBL_params {
  int32_t block_size;
  bool do_prefetch;

  uint32_t curr_idx;                // current index in the prev_access_block
  int32_t sequential_confidence_k;  // number of prev sequential accesses to be
                                    // considered as a sequential access
  obj_id_t* prev_access_block;      // prev k accessed
} OBL_params_t;

typedef struct OBL_init_params {
  int32_t block_size;
  int32_t sequential_confidence_k;
} OBL_init_params_t;

#define DO_PREFETCH(id)

#endif