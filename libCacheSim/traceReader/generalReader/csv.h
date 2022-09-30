//
//  csvReader.h
//  libCacheSim
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <stdbool.h>

#include "../../include/libCacheSim/reader.h"

typedef struct {
  struct csv_parser *csv_parser;

  /* used in cb1, each time a field is read, curr_field_idx incr */
  int curr_field_idx;

  int time_field_idx;
  int obj_id_field_idx;
  int obj_size_field_idx;
  bool has_header;
  unsigned char delimiter;

  void *request;
} csv_params_t;

bool csv_detect_obj_id_is_num(reader_t *const reader);

void csv_setup_reader(reader_t *const reader);

int csv_read_one_req(reader_t *const, request_t *const);

uint64_t csv_skip_N_elements(reader_t *const reader, const uint64_t N);

void csv_reset_reader(reader_t *reader);

void csv_set_no_eof(reader_t *reader);

#ifdef __cplusplus
}
#endif
