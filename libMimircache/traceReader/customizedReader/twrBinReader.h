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
  req->real_time = *(guint32 *) record;
  record += 4;
  req->obj_id_ptr = (gpointer) (*(guint64 *) record);
//  req->obj_id_ptr = (gpointer) (rand() % 1000000L);
  record += 8;
  guint32 kv_size = *(guint32 *) record;
  record += 4;
  guint32 op_ttl = *(guint32 *) record;

  guint32 key_size = (kv_size >> 22) & (0x00000400 - 1);
  guint32 val_size = kv_size & (0x00400000 - 1);

  guint32 op = (op_ttl >> 24) & (0x00000100 - 1);
  guint32 ttl = op_ttl & (0x01000000 - 1);

  req->obj_size = key_size + val_size;
  req->op = op;
  req->ttl = ttl;

  reader->base->mmap_offset += 20;
  return 0;
}


#ifdef __cplusplus
}
#endif

#endif //LIBMIMIRCACHE_TWRBINREADER_H
