/*
 * format
 * IQQiiiQhhhbbb
 *
 * struct reqEntryReuseCF {
 *     uint32_t real_time_;
 *     uint64_t obj_id_;
 *     uint64_t obj_size_;
 *     uint32_t ttl_;
 *     uint32_t age_;
 *     uint32_t hostname_;
 *     int64_t next_access_vtime_;
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

#include "../../../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int oracleCF1_setup(reader_t *reader) {
  reader->trace_type = ORACLE_CF1_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 49;
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static int oracleCF1_read_one_req(reader_t *reader, request_t *req) {
  char *record = read_bytes(reader);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint64_t *)(record + 12);
  if (req->obj_size > UINT32_MAX) {
    if (req->obj_size > 0xFFFFFFFF00000000)
      req->obj_size -= 0xFFFFFFFF00000000;
  }

  req->ttl = *(int32_t *)(record + 20);
  req->age = *(uint32_t *)(record + 24);
  req->hostname = *(uint32_t *)(record + 28);
  req->next_access_vtime = *(int64_t *)(record + 32);
  req->content_type = *(uint16_t *)(record + 40);
  req->extension = *(uint16_t *)(record + 42);
  req->n_level = *(uint16_t *)(record + 44);
  req->n_param = *(uint8_t *)(record + 46);
  req->method = *(uint8_t *)(record + 47);
  req->colo = *(uint8_t *)(record + 48);

  if (req->obj_size == 0 && reader->ignore_size_zero_req &&
      reader->read_direction == READ_FORWARD)
    return oracleCF1_read_one_req(reader, req);
  return 0;
}

#ifdef __cplusplus
}
#endif
