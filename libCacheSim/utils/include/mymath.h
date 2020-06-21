//
// Created by Juncheng Yang on 5/7/20.
//

#ifndef libCacheSim_MATHUTILS_H
#define libCacheSim_MATHUTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


static uint64_t rand_seed = 0;

////**
// * find the maximum value in an array
// * @param array
// * @param n_elem
// * @param max_elem
// * @param max_elem_idx
// */
//static inline void find_max_int64_t(int64_t *array, int64_t n_elem, int64_t* max_elem, int64_t* max_elem_idx) {
//  int64_t max = array[0], max_idx = 0;
//  for (int64_t i=0; i<n_elem; i++){
//    if (array[i] > max) {
//      max = array[i];
//      max_idx = i;
//    }
//  }
//  if (max_elem != NULL)
//    *max_elem = max;
//  if (max_elem_idx != NULL)
//    *max_elem_idx = max_idx;
//}



static inline void set_rand_seed(int64_t seed){
  rand_seed = seed;
}

/**
 * generate pseudo rand number, taken from LHD simulator
 * random number generator from Knuth MMIX
 * @return
 */
static inline uint64_t next_rand(){
  rand_seed = 6364136223846793005 * rand_seed + 1442695040888963407;
  return rand_seed;
}


#ifdef __cplusplus
}
#endif


#endif //libCacheSim_MATHUTILS_H
