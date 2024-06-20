
#pragma once
//
// Created by Juncheng Yang on 6/19/20.
//

#include "const.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>


namespace utilsStr {

/**
 * convert size to an appropriate string with unit, for example 1048576 will be
 * 1 MiB
 * @param size
 * @param str a 8 byte char array
 */
static inline void convert_size_to_str(long long size, char *str) {

    if (size >= TiB) {
        sprintf(str, "%.0lf TiB", (double) size / TiB);
    } else if (size >= GiB) {
        sprintf(str, "%.0lf GiB", (double) size / GiB);
    } else if (size >= MiB) {
        sprintf(str, "%.0lf MiB", (double) size / MiB);
    } else if (size >= KiB) {
        sprintf(str, "%.0lf KiB", (double) size / KiB);
    } else {
        sprintf(str, "%lld B", size);
    }
}

static inline uint64_t str_to_u64(const char *start, size_t len) {
    uint64_t n = 0;
    while (len--) {
        n = n * 10 + *start++ - '0';
    }
    return n;
}

}// namespace utilsStr