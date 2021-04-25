#pragma once

// oracle binary trace format

//struct reqEntryReuseAkamai {
//  uint32_t real_time;
//  uint64_t obj_id;
//  uint32_t obj_size;
//  int16_t customer;
//  int16_t bucket;
//  int16_t content;
//  int64_t reuse;
//}__attribute__((packed));


#include "../../include/libCacheSim.h"

static inline int oracleAkamai_setup(reader_t *reader) {
  reader->trace_type = ORACLE_BIN_TRACE;
  reader->item_size = 30;

  if (reader->file_size % reader->item_size != 0) {
    WARNING("trace file size %lu is not multiple of record size %lu\n",
            (unsigned long) reader->file_size,
            (unsigned long) reader->item_size);
  }

  reader->n_total_req = (uint64_t) reader->file_size / (reader->item_size);
  return 0;
}

static inline int oracleAkamai_read_one_req(reader_t *reader, request_t *req) {
  if (reader->mmap_offset >= reader->file_size) {
    req->valid = FALSE;
    return 1;
  }

  char *record = (reader->mapped_file + reader->mmap_offset);
  req->real_time = *(uint32_t *) record;
  req->obj_id_int = *(uint64_t *) (record + 4);
  req->obj_size = *(uint32_t *) (record + 12);
  req->customer_id = *(int16_t *) (record + 16);
  req->bucket_id = *(int16_t *) (record + 18);
  req->content_type = *(int16_t *) (record + 20);

  req->next_access_ts = *(int64_t *) (record + 22);

  reader->mmap_offset += reader->item_size;
  if (req->obj_size == 0)
    return oracleAkamai_read_one_req(reader, req);
  return 0;
}

