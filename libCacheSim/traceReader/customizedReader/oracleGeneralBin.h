#pragma once

/** oracle binary trace format
 *  struct {
 *    uint32_t real_time;
 *    uint64_t obj_id;
 *    uint32_t obj_size;
 *    int64_t next_access_ts;
 *  };
 *
 */

#include "../../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int oracleGeneralBin_setup(reader_t *reader) {
  reader->trace_type = ORACLE_GENERAL_BIN;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 24;
  reader->n_total_req = (uint64_t) reader->file_size / (reader->item_size);
  return 0;
}

static inline int oracleGeneralBin_read_one_req(reader_t *reader, request_t *req) {
  char *record = (reader->mapped_file + reader->mmap_offset);
  req->real_time = *(uint32_t *) record;
  req->obj_id_int = *(uint64_t *) (record + 4);
  req->obj_size = *(uint32_t *) (record + 12);
  req->next_access_ts = *(int64_t *) (record + 16);
  if (req->next_access_ts == -1) {
    req->next_access_ts = INT64_MAX;
  }

  reader->mmap_offset += reader->item_size;
  if (req->obj_size == 0)
    return oracleGeneralBin_read_one_req(reader, req);
  return 0;
}

#ifdef __cplusplus
}
#endif
