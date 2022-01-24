/*
 * format
 * struct {
 *  uint32_t real_time;
 *  uint64_t obj_id;
 *  uint32_t size;
 *  uint16_t customer;
 *  uint16_t bucket;
 *  uint16_t content_type;
 * }
 *
 */
#pragma once

#include <math.h>
#include "../../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int akamaiReader_setup(reader_t *reader) {
  reader->trace_type = AKAMAI_BIN_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 22;  /* IqIhhh */
  reader->n_total_req = (uint64_t) reader->file_size / (reader->item_size);
  return 0;
}

static inline int akamai_read_one_req(reader_t *reader, request_t *req) {
  char *record = (reader->mapped_file + reader->mmap_offset);
  req->real_time = *(uint32_t *) record;
  req->obj_id = *(uint64_t *) (record + 4);
  req->obj_size = *(uint32_t *) (record + 12);
  req->tenant_id = *(uint16_t *) (record + 16);
  req->ns = *(uint16_t *) (record + 18);
  req->content_type = *(uint16_t *) (record + 20);

  reader->mmap_offset += reader->item_size;

  return 0;
}

#ifdef __cplusplus
}
#endif
