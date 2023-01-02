

#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/*
 * standardBinIQI format
 * struct {
 *   uint32_t real_time;
 *   uint64_t obj_id;
 *   uint32_t size;
 * }
 *
 *
 * standardBinIII format
 * struct {
 *   uint32_t real_time;
 *   uint32_t obj_id;
 *   uint32_t size;
 * }
 *
 */

#include "../../include/libCacheSim/reader.h"
#include "binaryUtils.h"

static inline int standardBinIQQ_setup(reader_t *reader) {
  reader->trace_type = STANDARD_IQI_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 20;
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int standardBinIQQ_read_one_req(reader_t *reader,
                                              request_t *req) {
  char *record = read_bytes(reader);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint32_t *)(record + 12);

  return 0;
}

static inline int standardBinIQI_setup(reader_t *reader) {
  reader->trace_type = STANDARD_IQI_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 16;
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int standardBinIQI_read_one_req(reader_t *reader,
                                              request_t *req) {
  char *record = read_bytes(reader);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint32_t *)(record + 12);

  return 0;
}

static inline int standardBinIII_setup(reader_t *reader) {
  reader->trace_type = STANDARD_III_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 12;
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int standardBinIII_read_one_req(reader_t *reader,
                                              request_t *req) {
  char *record = read_bytes(reader);

  if (record == NULL) {
    req->valid = FALSE;
    return 1;
  }

  req->clock_time = *(uint32_t *)record;
  req->obj_id = *(uint64_t *)(record + 4);
  req->obj_size = *(uint32_t *)(record + 8);

  return 0;
}

static inline int standardBinIQIBH_setup(reader_t *reader) {
  reader->trace_type = STANDARD_IQIBH_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 19;
  reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
  reader->obj_id_is_num = true;
  return 0;
}

static inline int standardBinIQIBH_read_one_req(reader_t *reader,
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

  DEBUG_ASSERT(req->op != 0 && req->op < OP_INVALID);

  return 0;
}

#ifdef __cplusplus
}
#endif
