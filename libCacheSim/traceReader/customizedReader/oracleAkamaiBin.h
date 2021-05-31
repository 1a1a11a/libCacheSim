#pragma once

/*  oracle binary trace format
 *
 * struct reqEntryReuseAkamai {
 *   uint32_t real_time;
 *   uint64_t obj_id;
 *   uint32_t obj_size;
 *   int16_t customer;
 *   int16_t bucket;
 *   int16_t content;
 *   int64_t next_access_ts;
 * }__attribute__((packed));
 *
 */


#include "../../include/libCacheSim/reader.h"


#ifdef __cplusplus
extern "C" {
#endif

static inline int oracleAkamai_setup(reader_t *reader) {
  reader->trace_type = ORACLE_AKAMAI_BIN;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 30;
  reader->n_total_req = (uint64_t) reader->file_size / (reader->item_size);
  return 0;
}

static inline int oracleAkamai_read_one_req(reader_t *reader, request_t *req) {
  char *record = (reader->mapped_file + reader->mmap_offset);
  req->real_time = *(uint32_t *) record;
  req->obj_id = *(uint64_t *) (record + 4);
  req->obj_size = *(uint32_t *) (record + 12);
  req->customer_id = *(int16_t *) (record + 16) - 1;
  req->bucket_id = *(int16_t *) (record + 18) - 1;
  req->content_type = *(int16_t *) (record + 20) - 1;

  req->next_access_ts = *(int64_t *) (record + 22);

  reader->mmap_offset += reader->item_size;
  if (req->obj_size == 0)
    return oracleAkamai_read_one_req(reader, req);
  return 0;
}


#ifdef __cplusplus
}
#endif
