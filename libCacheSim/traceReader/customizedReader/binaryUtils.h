#pragma once

#ifdef SUPPORT_ZSTD_TRACE
#include "../generalReader/zstdReader.h"
#endif

/* read decompressed data file */
static inline char *_read_bytes(reader_t *reader) {
  if (reader->mmap_offset >= reader->file_size) {
    return NULL;
  }

  char *start = (reader->mapped_file + reader->mmap_offset);
  reader->mmap_offset += reader->item_size;

  return start;
}

#ifdef SUPPORT_ZSTD_TRACE
/* read zstd compressed data */
static inline char *_read_bytes_zstd(reader_t *reader) {
  char *start;
  size_t sz =
      zstd_reader_read_bytes(reader->zstd_reader_p, reader->item_size, &start);
  if (sz == 0) {
    if (reader->zstd_reader_p->status != MY_EOF) {
      ERROR("fail to read zstd trace\n");
    }
    return NULL;
  }

  return start;
}
#endif

static inline char *read_bytes(reader_t *reader) {
  char *start = NULL;
#ifdef SUPPORT_ZSTD_TRACE
  if (reader->is_zstd_file) {
    start = _read_bytes_zstd(reader);
  } else
#endif
  {
    start = _read_bytes(reader);
  }
  return start;
}