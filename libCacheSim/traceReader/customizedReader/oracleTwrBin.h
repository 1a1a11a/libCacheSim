#pragma once

#include "../../include/libCacheSim.h"



static inline int oracleTwr_setup(reader_t *reader) {
  reader->trace_type = ORACLE_TWR_TRACE;
  reader->item_size = 32;

  if (reader->file_size % reader->item_size != 0) {
    WARNING("trace file size %lu is not multiple of record size %lu\n",
            (unsigned long) reader->file_size,
            (unsigned long) reader->item_size);
  }

  reader->n_total_req = (uint64_t) reader->file_size / (reader->item_size);
  return 0;
}

static inline int oracleTwr_read_one_req(reader_t *reader, request_t *req) {
  if (reader->mmap_offset >= reader->file_size) {
    req->valid = FALSE;
    return 1;
  }

  char *record = (reader->mapped_file + reader->mmap_offset);
  req->real_time = *(uint32_t *) record;
  req->obj_id_int = *(uint64_t *) (record + 4);

  uint32_t key_size = *(uint16_t *) (record + 12);
  uint32_t val_size = *(uint32_t *) (record + 14);


  req->obj_size = key_size + val_size;
  req->op = (req_op_e)(*(uint16_t *) (record + 18));
  req->ttl = *(uint16_t *) (record + 20);
  req->ttl = 86400 * 30;
  req->next_access_ts = *(int64_t *) (record + 24);
//  if (req->next_access_ts == -1) {
//    req->next_access_ts = INT64_MAX;
//  }

  reader->mmap_offset += reader->item_size;
  if (req->obj_size == 0)
    return oracleTwr_read_one_req(reader, req);
  return 0;
}

