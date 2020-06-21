#pragma once

//
// Created by Juncheng Yang on 6/20/20.
//


#include "../../include/libCacheSim/reader.h"

bool find_line_ending(reader_t *const reader, char **line_end,
                      size_t *const line_len);

int go_back_two_lines(reader_t *const reader);

static inline obj_id_t str_to_obj_id(const char *start, size_t len) {
  obj_id_t n = 0;
  while (len--) {
    n = n * 10 + *start++ - '0';
  }
  return n;
}

