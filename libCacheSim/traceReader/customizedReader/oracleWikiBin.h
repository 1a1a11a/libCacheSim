#pragma once
#ifdef __cplusplus
extern "C" {
#endif


/*
 * wiki 2016 upload format
 * struct {
 *   uint64_t obj_id;
 *   uint32_t size;
 *   uint16_t content_type;
 *   int64_t next_access_ts;
 * }
 *
 *
 * wiki 2019 upload format
 * struct {
 *   uint32_t real_time;
 *   uint64_t obj_id;
 *   uint32_t size;
 *   uint16_t content_type;
 *   int64_t next_access_ts;
 * }
 *
 *
 * wiki 2019 txt format
 * struct {
 *   uint32_t real_time;
 *   uint64_t obj_id;
 *   uint32_t size;
 *   int64_t next_access_ts;
 * }
 *
 *
 */


#include "../../include/libCacheSim.h"

static inline int oracleWiki2016uReader_setup(reader_t *reader) {
  reader->trace_type = WIKI16u_BIN_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 22;
  reader->n_total_req = (uint64_t) reader->file_size / (reader->item_size);
  return 0;
}

static inline int oracleWiki2016u_read_one_req(reader_t *reader, request_t *req) {
  char *record = (reader->mapped_file + reader->mmap_offset);
  req->real_time = 0;
  req->obj_id_int = *(uint64_t *) (record);
  req->obj_size = *(uint32_t *) (record + 8);
  req->content_type = *(uint16_t *) (record + 12);
  req->next_access_ts = *(uint64_t *) (record + 14);

  reader->mmap_offset += reader->item_size;

  return 0;
}

static inline int oracleWiki2019uReader_setup(reader_t *reader) {
  reader->trace_type = WIKI19u_BIN_TRACE;
  reader->item_size = 26;
  reader->n_total_req = (uint64_t) reader->file_size / (reader->item_size);
  return 0;
}

static inline int oracleWiki2019u_read_one_req(reader_t *reader, request_t *req) {
  char *record = (reader->mapped_file + reader->mmap_offset);
  req->real_time = *(uint32_t *) record;
  req->obj_id_int = *(uint64_t *) (record + 4);
  req->obj_size = *(uint32_t *) (record + 12);
  req->content_type = *(uint16_t *) (record + 16);
  req->next_access_ts = *(uint64_t *) (record + 18);

  reader->mmap_offset += reader->item_size;

  return 0;
}

static inline int oracleWiki2019tReader_setup(reader_t *reader) {
  reader->trace_type = WIKI19t_BIN_TRACE;
  reader->item_size = 24;
  reader->n_total_req = (uint64_t) reader->file_size / (reader->item_size);
  return 0;
}

static inline int oracleWiki2019t_read_one_req(reader_t *reader, request_t *req) {
  char *record = (reader->mapped_file + reader->mmap_offset);
  req->real_time = *(uint32_t *) record;
  req->obj_id_int = *(uint64_t *) (record + 4);
  req->obj_size = *(uint32_t *) (record + 12);
  req->next_access_ts = *(uint64_t *) (record + 16);

  reader->mmap_offset += reader->item_size;

  return 0;
}

#ifdef __cplusplus
}
#endif


