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

static __thread uint64_t rand_seed = 0;

static inline void set_rand_seed(uint64_t seed) {
  rand_seed = seed;
}

/**
 * generate pseudo rand number, taken from LHD simulator
 * random number generator from Knuth MMIX
 * @return
 */
static inline uint64_t next_rand() {
  rand_seed = 6364136223846793005 * rand_seed + 1442695040888963407;
  return rand_seed & 0xfffffffffffffffful;
}

#ifdef __cplusplus
}
#endif

#endif //libCacheSim_MATHUTILS_H
