
#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "libCacheSim/const.h"

/**
 * convert size to an appropriate string with unit, for example 1048576 will be
 * 1 MiB
 * @param size
 * @param str a 8 byte char array
 */
void convert_size_to_str(unsigned long long size, char *str, size_t str_len) {
  if (size >= TiB) {
    snprintf(str, str_len, "%.0lfTiB", (double)size / TiB);
  } else if (size >= GiB) {
    snprintf(str, str_len, "%.0lfGiB", (double)size / GiB);
  } else if (size >= MiB) {
    snprintf(str, str_len, "%.0lfMiB", (double)size / MiB);
  } else if (size >= KiB) {
    snprintf(str, str_len, "%.0lfKiB", (double)size / KiB);
  } else {
    snprintf(str, str_len, "%lldB", size);
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
  const char *s = strrchr(path, '/');
  if (!s)
    return path;
  else
    return s + 1;
}

void check_key_value_params(const char *owner, const char *params) {
  if (params == NULL) return;

  const char *token = params;
  while (*token != '\0') {
    // the parsers skip spaces after a comma, and ignore empty entries
    while (*token == ' ') token++;
    const char *comma = strchr(token, ',');
    size_t len = comma != NULL ? (size_t)(comma - token) : strlen(token);

    if (len > 0) {
      const char *eq = memchr(token, '=', len);
      const char *error = NULL;
      if (eq == NULL) {
        bool is_flag =
            (len == strlen("print") && strncasecmp(token, "print", len) == 0) ||
            (len == strlen("default") &&
             strncasecmp(token, "default", len) == 0);
        if (!is_flag) error = "expected key=value";
      } else if (eq == token) {
        error = "expected key=value";
      } else if (eq + 1 == token + len) {
        error = "expected a nonempty value";
      }

      if (error != NULL) {
        // name the key when there is one, otherwise the whole entry
        int name_len =
            (eq != NULL && eq != token) ? (int)(eq - token) : (int)len;
        fprintf(stderr, "error: %s parameter '%.*s': %s\n", owner, name_len,
                token, error);
        exit(EXIT_FAILURE);
      }
    }

    token = comma != NULL ? comma + 1 : token + len;
  }
}

#ifdef __cplusplus
}
#endif
