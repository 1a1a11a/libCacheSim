
#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/libCacheSim/const.h"

/**
 * convert size to an appropriate string with unit, for example 1048576 will be
 * 1 MiB
 * @param size
 * @param str a 8 byte char array
 */
void convert_size_to_str(unsigned long long size, char *str) {
  if (size >= TiB) {
    sprintf(str, "%.0lfTiB", (double)size / TiB);
  } else if (size >= GiB) {
    sprintf(str, "%.0lfGiB", (double)size / GiB);
  } else if (size >= MiB) {
    sprintf(str, "%.0lfMiB", (double)size / MiB);
  } else if (size >= KiB) {
    sprintf(str, "%.0lfKiB", (double)size / KiB);
  } else {
    sprintf(str, "%lldB", size);
  }
}

/**
 * @brief convert a string to a uint64_t
 *
 * @param start
 * @param len
 * @return uint64_t
 */
uint64_t str_to_u64(const char *start, size_t len) {
  uint64_t n = 0;
  while (len--) {
    n = n * 10 + *start++ - '0';
  }
  return n;
}

/* replace all matching char in a string */
char *replace_char(char *str, char find, char replace) {
  char *current_pos = strchr(str, find);
  while (current_pos) {
    *current_pos = replace;
    current_pos = strchr(current_pos + 1, find);
  }
  return str;
}

const char *mybasename(char const *path) {
  char *s = strrchr(path, '/');
  if (!s)
    return path;
  else
    return s + 1;
}

#ifdef __cplusplus
}
#endif
