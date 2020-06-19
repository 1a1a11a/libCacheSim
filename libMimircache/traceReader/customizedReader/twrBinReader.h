//
// Created by Juncheng Yang on 4/18/20.
//

#ifndef LIBMIMIRCACHE_TWRBINREADER_H
#define LIBMIMIRCACHE_TWRBINREADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../include/mimircache.h"
#include "readerInternal.h"


int twrReader_setup(reader_t *reader) {
  reader->base->trace_type = TWR_TRACE;
  reader->base->item_size = 20;

  if (reader->base->file_size % reader->base->item_size != 0) {
    WARNING("trace file size %lu is not multiple of record size %lu\n",
            (unsigned long) reader->base->file_size,
            (unsigned long) reader->base->item_size);
  }

  reader->base->n_total_req = (guint64) reader->base->file_size / (reader->base->item_size);
  return 0;
}

static inline int twr_read(reader_t *reader, request_t *req) {
  if (reader->base->mmap_offset >= reader->base->file_size) {
    req->valid = FALSE;
    return 0;
  }

  char *record = (reader->base->mapped_file + reader->base->mmap_offset);
  req->real_time = *(uint32_t *) record;
  record += 4;

  req->obj_id_int = *(uint64_t *) record;
  record += 8;

  uint32_t kv_size = *(uint32_t *) record;
  record += 4;
  uint32_t op_ttl = *(uint32_t *) record;

  uint32_t key_size = (kv_size >> 22) & (0x00000400 - 1);
  uint32_t val_size = kv_size & (0x00400000 - 1);

  uint32_t op = (op_ttl >> 24) & (0x00000100 - 1);
  uint32_t ttl = op_ttl & (0x01000000 - 1);

  req->obj_size = key_size + val_size;
  req->op = op;
#ifdef SUPPORT_TTL
  req->ttl = ttl;
#endif
  
  reader->base->mmap_offset += 20;
  if (req->obj_size == 0)
    return twr_read(reader, req);
  return 0;
}


#ifdef __cplusplus
}
#endif

#endif //LIBMIMIRCACHE_TWRBINREADER_H
