
#include "zstdReader.h"

#include <assert.h>
#include <errno.h>  // errno
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // strerror
#include <sys/stat.h>
#include <zstd.h>

#include "../../include/libCacheSim/logging.h"

#define LINE_DELIM '\n'

zstd_reader_t *create_zstd_reader(const char *trace_path) {
  zstd_reader_t *reader = malloc(sizeof(zstd_reader_t));

  reader->ifile = fopen(trace_path, "rb");
  if (reader->ifile == NULL) {
    printf("cannot open %s\n", trace_path);
    exit(1);
  }

  reader->buff_in_sz = ZSTD_DStreamInSize();
  reader->buff_in = malloc(reader->buff_in_sz);
  reader->input.src = reader->buff_in;
  reader->input.size = 0;
  reader->input.pos = 0;

  reader->buff_out_sz = ZSTD_DStreamOutSize() * 2;
  reader->buff_out = malloc(reader->buff_out_sz);
  reader->output.dst = reader->buff_out;
  reader->output.size = reader->buff_out_sz;
  reader->output.pos = 0;

  reader->buff_out_read_pos = 0;
  reader->status = 0;

  reader->zds = ZSTD_createDStream();

  DEBUG("create zstd reader %s\n", trace_path);
  return reader;
}

void free_zstd_reader(zstd_reader_t *reader) {
  ZSTD_freeDStream(reader->zds);
  free(reader->buff_in);
  free(reader->buff_out);
  free(reader);
  DEBUG("free zstd reader\n");
}

void reset_zstd_reader(zstd_reader_t *reader) {
  ZSTD_freeDStream(reader->zds);
  reader->zds = ZSTD_createDStream();
  fseek(reader->ifile, 0, SEEK_SET);
  reader->input.size = 0;
  reader->input.pos = 0;
  memset(reader->buff_in, 0, reader->buff_in_sz);
  reader->output.pos = 0;
  reader->buff_out_read_pos = 0;
  memset(reader->buff_out, 0, reader->buff_out_sz);
  reader->status = 0;
}

size_t _read_from_file(zstd_reader_t *reader) {
  size_t read_sz;
  read_sz = fread(reader->buff_in, 1, reader->buff_in_sz, reader->ifile);
  if (read_sz < reader->buff_in_sz) {
    if (feof(reader->ifile)) {
      reader->status = MY_EOF;
    } else {
      assert(ferror(reader->ifile));
      reader->status = ERR;
      return 0;
    }
  }
  //  DEBUG("read %zu bytes from file\n", read_sz);

  reader->input.size = read_sz;
  reader->input.pos = 0;

  return read_sz;
}

rstatus _decompress_from_buff(zstd_reader_t *reader) {
  /* move the unread decompressed data to the head of buff_out */
  void *buff_start = reader->buff_out + reader->buff_out_read_pos;
  size_t buff_left_sz = reader->output.pos - reader->buff_out_read_pos;
  memmove(reader->buff_out, buff_start, buff_left_sz);
  reader->output.pos = buff_left_sz;
  reader->buff_out_read_pos = 0;
  size_t old_pos = buff_left_sz;

  if (reader->input.pos >= reader->input.size) {
    size_t read_sz = _read_from_file(reader);
    if (read_sz == 0) {
      if (reader->status == MY_EOF) {
        return MY_EOF;
      } else {
        ERROR("read from file error\n");
        return ERR;
      }
    }
  }

  size_t const ret = ZSTD_decompressStream(reader->zds, &(reader->output), &(reader->input));
  if (ret != 0) {
    if (ZSTD_isError(ret)) {
      printf("%zu\n", ret);
      WARN("zstd decompression error: %s\n", ZSTD_getErrorName(ret));
    }
  }
  //  DEBUG("decompress %zu - %zu bytes\n", reader->output.pos, old_pos);

  return OK;
}

/**
    *line_start points to the start of the new line
    *line_end   points to the \n

    @return the number of bytes read (include line ending byte)
**/
size_t zstd_reader_read_line(zstd_reader_t *reader, char **line_start, char **line_end) {
  bool has_data_in_line_buff = false;

  if (reader->buff_out_read_pos < reader->output.pos) {
    /* there are still content in output buffer */
    *line_start = reader->buff_out + reader->buff_out_read_pos;
    void *buff_start = reader->buff_out + reader->buff_out_read_pos;
    size_t buff_left_sz = reader->output.pos - reader->buff_out_read_pos;
    *line_end = memchr(buff_start, LINE_DELIM, buff_left_sz);
    if (*line_end == NULL) {
      /* cannot find end of line, copy left over bytes, and decompress the next
       * frame */
      has_data_in_line_buff = true;
    } else {
      /* find a line in buff_out */
      assert(**line_end == LINE_DELIM);
      size_t sz = *line_end - *line_start + 1;
      reader->buff_out_read_pos += sz;

      return sz;
    }
  }

  rstatus status = _decompress_from_buff(reader);
  if (status != OK) {
    if (status == MY_EOF && has_data_in_line_buff) {
      *(((char *)(reader->buff_out)) + reader->output.pos) = '\n';
    } else {
      return 0;
    }
  } else if (reader->output.pos < reader->output.size / 4) {
    /* input buffer does not have enough content, read more from file */
    status = _decompress_from_buff(reader);
  }

  *line_start = reader->buff_out + reader->buff_out_read_pos;
  *line_end = memchr(*line_start, LINE_DELIM, reader->output.pos);
  // printf("start at %d %d end %d %d\n", reader->buff_out_read_pos,
  // **line_start, *line_end - *line_start, **line_end);
  assert(*line_end != NULL);
  assert(**line_end == LINE_DELIM);
  size_t sz = *line_end - *line_start + 1;
  reader->buff_out_read_pos = sz;

  return sz;
}

/**
 * read n_byte from reader, decompress if needed, data_start points to the new
 * data
 *
 * return the number of available bytes
 *
 * @param reader
 * @param n_byte
 * @param data_start
 * @return
 */
size_t zstd_reader_read_bytes(zstd_reader_t *reader, size_t n_byte, char **data_start) {
  size_t sz = 0;
  while (reader->buff_out_read_pos + n_byte > reader->output.pos) {
    rstatus status = _decompress_from_buff(reader);

    if (status != OK) {
      if (status != MY_EOF) {
        ERROR("error decompress file\n");
      } else {
        /* end of file */
        return 0;
      }
      break;
    }
  }

  if (reader->buff_out_read_pos + n_byte <= reader->output.pos) {
    sz = n_byte;
    *data_start = ((char *)reader->buff_out) + reader->buff_out_read_pos;
    reader->buff_out_read_pos += n_byte;

    return sz;
  } else {
    ERROR("do not have enough bytes %zu < %zu\n", reader->output.pos - reader->buff_out_read_pos, n_byte);

    return sz;
  }
}