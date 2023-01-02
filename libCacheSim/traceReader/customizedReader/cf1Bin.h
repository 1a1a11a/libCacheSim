/*
 * format
 * IQQiiihhhbbb
 *
 * struct reqEntryReuseCF {
 *     uint32_t real_time_;
 *     uint64_t obj_id_;
 *     uint64_t obj_size_;
 *     uint32_t ttl_;
 *     uint32_t age_;
 *     uint32_t hostname_;
 *
 *     uint16_t content_type_;
 *     uint16_t extension_;
 *     uint16_t n_level_;
 *     uint8_t n_param_;
 *     uint8_t method_;
 *     uint8_t colo_;
 * }
 *
 *
 */

#pragma once

#include "../../include/libCacheSim/reader.h"
#include "binaryUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int cf1Reader_setup(reader_t *reader) {
  reader->trace_type = CF1_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 41;
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static int cf1_read_one_req(reader_t *reader, request_t *req) {
  char *record = read_bytes(reader);

  if (record == NULL) {
    req->valid = false;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint64_t *)(record + 12);
  req->ttl = *(int32_t *)(record + 20);
  req->age = *(uint32_t *)(record + 24);
  req->hostname = *(uint32_t *)(record + 28);
  req->content_type = *(uint16_t *)(record + 32);
  req->extension = *(uint16_t *)(record + 34);
  req->n_level = *(uint16_t *)(record + 36);
  req->n_param = *(uint8_t *)(record + 38);
  req->method = *(uint8_t *)(record + 39);
  req->colo = *(uint8_t *)(record + 40);

  if (req->obj_size == 0 && reader->ignore_size_zero_req &&
      reader->read_direction == READ_FORWARD)
    return cf1_read_one_req(reader, req);

  return 0;
}

#ifdef __cplusplus
}
#endif
