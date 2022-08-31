//
// Created by Juncheng on 6/1/21.
//

#include "include/mymath.h"

#include <inttypes.h>

#include "../include/libCacheSim/logging.h"

__thread uint64_t rand_seed = 0;

void set_rand_seed(uint64_t seed) { rand_seed = seed; }
