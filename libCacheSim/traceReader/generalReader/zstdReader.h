#pragma once

#include <stdio.h>
#include <zstd.h>

#include "../../include/libCacheSim/enum.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct zstd_reader {
  FILE *ifile;
  ZSTD_DStream *zds;

  size_t buff_in_sz;
  void *buff_in;
  size_t buff_out_sz;
  void *buff_out;

  size_t buff_out_read_pos;

  ZSTD_inBuffer input;
  ZSTD_outBuffer output;

  rstatus status;
} zstd_reader_t;

zstd_reader_t *create_zstd_reader(const char *trace_path);

void free_zstd_reader(zstd_reader_t *reader);

void reset_zstd_reader(zstd_reader_t *reader);

size_t zstd_reader_read_line(zstd_reader_t *reader, char **line_start,
                             char **line_end);

/* read n_byte from reader, decompress if needed, data_start points to the new
 * data */
size_t zstd_reader_read_bytes(zstd_reader_t *reader, size_t n_byte,
                              char **data_start);

#ifdef __cplusplus
}
#endif
