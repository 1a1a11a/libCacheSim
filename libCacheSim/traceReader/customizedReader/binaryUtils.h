#pragma once

#ifdef SUPPORT_ZSTD_TRACE
#include "../generalReader/zstdReader.h"
#endif

#include "../../include/libCacheSim/reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/* read decompressed data file */
static inline char *_read_bytes(reader_t *reader, size_t size) {
  if (reader->mmap_offset >= reader->file_size) {
    return NULL;
  }

  char *start = (reader->mapped_file + reader->mmap_offset);
  reader->mmap_offset += size;

  return start;
}

#ifdef SUPPORT_ZSTD_TRACE
/* read zstd compressed data */
static inline char *_read_bytes_zstd(reader_t *reader, size_t size) {
  char *start;
  size_t sz =
      zstd_reader_read_bytes(reader->zstd_reader_p, size, &start);
  if (sz == 0) {
    if (reader->zstd_reader_p->status != MY_EOF) {
      ERROR("fail to read zstd trace\n");
    }
    return NULL;
  }

  return start;
}
#endif

static inline char *read_bytes(reader_t *reader, size_t size) {
  char *start = NULL;
#ifdef SUPPORT_ZSTD_TRACE
  if (reader->is_zstd_file) {
    start = _read_bytes_zstd(reader, size);
  } else
#endif
  {
    start = _read_bytes(reader, size);
  }
  return start;
}

#ifdef __cplusplus
}
#endif
