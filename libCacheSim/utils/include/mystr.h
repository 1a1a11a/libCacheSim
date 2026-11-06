#pragma once
//
// Created by Juncheng Yang on 6/19/20.
//

#include <inttypes.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * convert size to an appropriate string with unit, for example 1048576 will be
 * 1 MiB
 * @param size
 * @param str a 8 byte char array
 */
void convert_size_to_str(unsigned long long size, char *str, size_t str_len);

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

/**
 * @brief exit with a message unless params is a list of key=value entries
 *
 * Algorithm parameters are comma-separated key=value entries. The parsers
 * hand the value straight to strtod, atoi or strcasecmp, so an entry without
 * one (`-e small-size-ratio`) used to crash them. Every entry needs a
 * nonempty key and value, except the bare "print" and "default" flags. Empty
 * entries and spaces after a comma are allowed, as the parsers skip them.
 *
 * @param owner the algorithm, named in the error message
 * @param params the parameter string, may be NULL
 */
void check_key_value_params(const char *owner, const char *params);

#ifdef __cplusplus
}
#endif
