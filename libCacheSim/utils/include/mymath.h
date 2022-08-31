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
static inline uint64_t next_rand() {
  rand_seed = 6364136223846793005 * rand_seed + 1442695040888963407;
  return rand_seed;
}

#ifdef __cplusplus
}
#endif
