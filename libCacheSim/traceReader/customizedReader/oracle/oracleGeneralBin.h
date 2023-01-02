#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/*
 * oracle binary trace format
 *
 *  struct {
 *    uint32_t real_time;
 *    uint64_t obj_id;
 *    uint32_t obj_size;
 *    int64_t next_access_vtime;
 *  };
 *
 *
 */

#include "../../../include/libCacheSim/reader.h"

static inline int oracleGeneralBin_setup(reader_t *reader) {
  reader->trace_type = ORACLE_GENERAL_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 24;
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int oracleGeneralBin_read_one_req(reader_t *reader,
                                                request_t *req) {
  char *record = read_bytes(reader);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint32_t *)(record + 12);
  req->next_access_vtime = *(int64_t *)(record + 16);
  if (req->next_access_vtime == -1) {
    req->next_access_vtime = INT64_MAX;
  }

  if (req->obj_size == 0 && reader->ignore_size_zero_req &&
      reader->read_direction == READ_FORWARD) {
    return oracleGeneralBin_read_one_req(reader, req);
  }
  return 0;
}

static inline int oracleGeneralOpNS_setup(reader_t *reader) {
  reader->trace_type = ORACLE_GENERALOPNS_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 27;
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int oracleGeneralOpNS_read_one_req(reader_t *reader,
                                                 request_t *req) {
  char *record = read_bytes(reader);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint32_t *)(record + 12);
  req->op = *(uint8_t *)(record + 16);
  req->ns = *(uint16_t *)(record + 17);
  req->next_access_vtime = *(int64_t *)(record + 19);
  if (req->next_access_vtime == -1) {
    req->next_access_vtime = INT64_MAX;
  }

  if (req->obj_size == 0 && reader->ignore_size_zero_req &&
      reader->read_direction == READ_FORWARD) {
    return oracleGeneralOpNS_read_one_req(reader, req);
  }
  return 0;
}

#ifdef __cplusplus
}
#endif
