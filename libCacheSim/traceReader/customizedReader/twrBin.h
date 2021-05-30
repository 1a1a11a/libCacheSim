//
// Created by Juncheng Yang on 4/18/20.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../../include/libCacheSim/reader.h"


static inline int twrReader_setup(reader_t *reader) {
  reader->trace_type = TWR_BIN_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 20;
  reader->n_total_req = (guint64) reader->file_size / (reader->item_size);
  return 0;
}

static inline int twr_read_one_req(reader_t *reader, request_t *req) {
  char *record = (reader->mapped_file + reader->mmap_offset);
  req->real_time = *(uint32_t *) record;
  req->obj_id_int = *(uint64_t *) (record + 4);

  uint32_t kv_size = *(uint32_t *) (record + 12);
  uint32_t op_ttl = *(uint32_t *) (record + 16);

  uint32_t key_size = (kv_size >> 22) & (0x00000400 - 1);
  uint32_t val_size = kv_size & (0x00400000 - 1);

  uint32_t op = (op_ttl >> 24) & (0x00000100 - 1);
  uint32_t ttl = op_ttl & (0x01000000 - 1);

  req->obj_size = key_size + val_size;
  req->op = op;
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  req->ttl = ttl;
#endif
  
  reader->mmap_offset += reader->item_size;
  if (req->obj_size == 0)
    return twr_read_one_req(reader, req);
  return 0;
}


#ifdef __cplusplus
}
#endif

