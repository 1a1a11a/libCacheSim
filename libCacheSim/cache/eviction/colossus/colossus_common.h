#ifndef LIBCACHESIM_COLOSSUS_COMMON_H
#define LIBCACHESIM_COLOSSUS_COMMON_H

#include "libCacheSim/evictionAlgo.h"

typedef struct colossus_segment {
  struct colossus_segment *prev; /* toward the tail (older) */
  struct colossus_segment *next; /* toward the head (newer) */
  cache_obj_t *q_head; /* most recently appended block in this segment */
  cache_obj_t *q_tail; /* earliest appended block in this segment */
  int64_t n_byte;
  int64_t n_obj;
} colossus_segment_t;

#endif
