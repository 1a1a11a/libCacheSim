//
// Created by Juncheng Yang on 5/7/20.
//

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../include/libCacheSim/logging.h"

#ifdef __cplusplus
extern "C" {
#endif

extern __thread uint64_t rand_seed;

void set_rand_seed(uint64_t seed);

/**
 * generate pseudo rand number, taken from LHD simulator
 * random number generator from Knuth MMIX
 * @return
 */
static inline uint64_t next_rand(void) {
  rand_seed = 6364136223846793005 * rand_seed + 1442695040888963407;
  return rand_seed;
}

static inline long long next_power_of_2(long long N) {
  // if N is a power of two simply return it
  if (!(N & (N - 1))) return N;
  // else set only the left bit of most significant bit
  return 0x8000000000000000 >> (__builtin_clzll(N) - 1);
}

static inline uint64_t next_power_of_2_v2(uint64_t n) {
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n |= n >> 32;
  n++;
  return n;
}

static inline unsigned long long log2_ull(unsigned long long n) {
  unsigned long long r = 0;
  while (n >>= 1) r++;
  return r;
}

static inline unsigned long long log2_v2(unsigned long long n) {
  return (unsigned long long)(8 * sizeof(unsigned long long) -
                              __builtin_clzll((n)));
}

#ifdef __cplusplus
}
#endif
