//
// Created by Juncheng on 6/1/21.
//

#include "include/mymath.h"

#include <inttypes.h>

#include "../include/libCacheSim/logging.h"

__thread uint64_t rand_seed = 0;
__thread __uint128_t g_lehmer64_state = 0xdeadbeef;


void set_rand_seed(uint64_t seed) { rand_seed = seed; g_lehmer64_state = seed; }

void linear_regression(double* x, double* y, int n, double* slope, double* intercept) {
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;

    // Calculate sums
    for (int i = 0; i < n; i++) {
        sum_x += x[i];
        sum_y += y[i];
        sum_xy += x[i] * y[i];
        sum_xx += x[i] * x[i];
    }

    // Calculate slope and intercept
    *slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
    *intercept = (sum_y - *slope * sum_x) / n;
}
