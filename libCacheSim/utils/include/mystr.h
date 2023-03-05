#pragma once
//
// Created by Juncheng Yang on 6/19/20.
//

#include <inttypes.h>

/**
 * convert size to an appropriate string with unit, for example 1048576 will be
 * 1 MiB
 * @param size
 * @param str a 8 byte char array
 */
void convert_size_to_str(unsigned long long size, char *str);

/**
 * @brief convert a string to a uint64_t
 *
 * @param start
 * @param len
 * @return uint64_t
 */
uint64_t str_to_u64(const char *start, size_t len);

/* replace all matching char in a string */
char *replace_char(char *str, char find, char replace);

const char *mybasename(const char *path);