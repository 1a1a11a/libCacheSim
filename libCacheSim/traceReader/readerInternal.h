#pragma once


#include "../include/libCacheSim/reader.h"

bool find_line_ending(reader_t *reader, char **next_line, size_t *line_len);

int go_back_two_lines(reader_t *reader);

static inline obj_id_t str_to_obj_id(const char *start, size_t len) {
  obj_id_t n = 0;
  while (len--) {
    n = n * 10 + *start++ - '0';
  }
  return n;
}

