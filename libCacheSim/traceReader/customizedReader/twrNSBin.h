/*
 * format
 *    IQIIH
 * struct {
 *  int32_t real_time;
 *  int64_t obj_id;
 *  int32_t key_size:10;
 *  int32_t value_size:22;
 *  int32_t op:8;
 *  int32_t ttl:24;
 *  int16_t namespace;
 *  }
 *
 */

#pragma once

#include "../../include/libCacheSim/reader.h"
#include "binaryUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int twrNSReader_setup(reader_t *reader) {
  reader->trace_type = TWRNS_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 22;
  reader->obj_id_is_num = true;
  return 0;
}

static inline int twrNS_read_one_req(reader_t *reader, request_t *req) {
  char *record = read_bytes(reader, reader->item_size);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  uint32_t kv_size = *(uint32_t *)(record + 12);
  uint32_t op_ttl = *(uint32_t *)(record + 16);
  req->ns = *(uint16_t *)(record + 20);

  uint32_t key_size = (kv_size >> 22) & (0x00000400 - 1);
  uint32_t val_size = kv_size & (0x00400000 - 1);

  uint32_t op = ((op_ttl >> 24) & (0x00000100 - 1));
  uint32_t ttl = op_ttl & (0x01000000 - 1);

  req->key_size = key_size;
  req->val_size = val_size;
  req->obj_size = key_size + val_size;
  req->op = (req_op_e)(op);
  req->ttl = (int32_t)ttl;

  if (req->val_size == 0 && reader->ignore_size_zero_req &&
      (req->op == OP_GET || req->op == OP_GETS) &&
      reader->read_direction == READ_FORWARD)
    return twrNS_read_one_req(reader, req);

  return 0;
}

#ifdef __cplusplus
}
#endif
