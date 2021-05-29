

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


#include "../../include/libCacheSim.h"



static inline int standardBinIQI_setup(reader_t *reader) {
  reader->trace_type = STANDARD_BIN_IQI_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 16;
  reader->n_total_req = (uint64_t) reader->file_size / (reader->item_size);
  return 0;
}

static inline int standardBinIQI_read_one_req(reader_t *reader, request_t *req) {
  char *record = (reader->mapped_file + reader->mmap_offset);
  req->real_time = *(uint32_t *) record;
  req->obj_id_int = *(uint64_t *) (record + 4);
  req->obj_size = *(uint32_t *) (record + 12);

  reader->mmap_offset += reader->item_size;

  return 0;
}


static inline int standardBinIII_setup(reader_t *reader) {
  reader->trace_type = STANDARD_BIN_III_TRACE;
  reader->trace_format = BINARY_TRACE_FORMAT;
  reader->item_size = 12;
  reader->n_total_req = (uint64_t) reader->file_size / (reader->item_size);
  return 0;
}

static inline int standardBinIII_read_one_req(reader_t *reader, request_t *req) {
  char *record = (reader->mapped_file + reader->mmap_offset);
  req->real_time = *(uint32_t *) record;
  req->obj_id_int = *(uint64_t *) (record + 4);
  req->obj_size = *(uint32_t *) (record + 8);

  reader->mmap_offset += reader->item_size;

  return 0;
}


#ifdef __cplusplus
}
#endif


