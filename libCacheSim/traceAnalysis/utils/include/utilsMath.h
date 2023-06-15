//
// Created by Juncheng Yang on 5/7/20.
//

#pragma once

#include <algorithm>
#include <cinttypes>
#include <numeric>
#include <vector>

namespace mathUtils {
static unsigned __int128 rand_seed = 0;

static inline void set_rand_seed(int64_t seed) {
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

template<typename T>
static inline T mean(const std::vector<T> &v, size_t ignore_n_first, size_t ignore_n_last) {
    if (v.size() == 0) {
        return 0;
    }
    if (v.size() <= ignore_n_first + ignore_n_last) {
        return 0;
    }

    T sum = accumulate(v.begin() + ignore_n_first, v.end() - ignore_n_last, (T) 0.0);
    return sum / (v.size() - ignore_n_first - ignore_n_last);
}

template<typename T>
static inline T median(const std::vector<T> &v) {
    if (v.size() == 0) {
        return 0;
    }

    std::vector<T> copy = v;
    sort(copy.begin(), copy.end());
    if (copy.size() % 2 == 0) {
        return (copy.at(copy.size() / 2 - 1) + copy.at(copy.size() / 2)) / 2;
    } else {
        return copy.at(copy.size() / 2);
    }
}

template<typename T>
static inline T stdev(const std::vector<T> &v, size_t ignore_n_first, size_t ignore_n_last) {
    if (v.size() <= 1) {
        return (T) 0.0;
    }
    if (v.size() <= ignore_n_first + ignore_n_last) {
        return 0;
    }

    T mean_ = mean(v, ignore_n_first, ignore_n_last);
    T sum = (T) 0.0;
    for (auto it = v.begin() + ignore_n_first; it != v.end() - ignore_n_last; it++) {
        sum += (*it - mean_) * (*it - mean_);
    }
    return (T) sqrt(sum / (v.size() - ignore_n_first - ignore_n_last));
}

}// namespace mathUtils
