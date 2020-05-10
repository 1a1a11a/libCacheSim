//
// Created by Juncheng Yang on 5/7/20.
//

#ifndef LIBMIMIRCACHE_MATHUTILS_H
#define LIBMIMIRCACHE_MATHUTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>

static guint64 rand_seed = 0;

static inline void find_max_gint64(gint64 *array, gint64 n_elem, gint64* max_elem, gint64* max_elem_idx) {
  gint64 max = array[0], max_idx = 0;
  for (gint64 i=0; i<n_elem; i++){
    if (array[i] > max) {
      max = array[i];
      max_idx = i;
    }
  }
  if (max_elem != NULL)
    *max_elem = max;
  if (max_elem_idx != NULL)
    *max_elem_idx = max_idx;
}



static inline void set_rand_seed(gint64 seed){
  rand_seed = seed;
}

/**
 * generate pseudo rand number, taken from LHD simulator
 * random number generator from Knuth MMIX
 * @return
 */
static inline guint64 next_rand(){
  rand_seed = 6364136223846793005 * rand_seed + 1442695040888963407;
  return rand_seed;
}


#ifdef __cplusplus
}
#endif


#endif //LIBMIMIRCACHE_MATHUTILS_H
