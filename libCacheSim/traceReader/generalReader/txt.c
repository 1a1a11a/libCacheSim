
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>

#include "../readerInternal.h"

int txt_read_one_req(reader_t *const reader, request_t *const req) {
  char **buf_ptr = (char **)&reader->line_buf;
  size_t *buf_size_ptr = &reader->line_buf_size;
  ssize_t read_size = getline(buf_ptr, buf_size_ptr, reader->file);
  VVERBOSE("read \"%s\", first char %d, read size %zu, curr pos %zu\n", reader->line_buf, reader->line_buf[0], read_size, ftell(reader->file));

  while (read_size == 1 && reader->line_buf[0] == '\n') {
    // empty line
    DEBUG("skip an empty line\n");
    read_size = getline(buf_ptr, buf_size_ptr, reader->file);
  }

  if (read_size == -1) {
    DEBUG("reach end of file\n");
    req->valid = false;
    return 1;
  }
  if (reader->obj_id_is_num) {
    char *end;
    req->obj_id = strtoull(reader->line_buf, &end, 0);
    if (req->obj_id == 0 && end == reader->line_buf) {
      ERROR("invalid object id, line: \"%s\", read size %ld\n", reader->line_buf,
            read_size);
    }
  } else {
    if (reader->line_buf[read_size - 1] == '\n') {
      reader->line_buf[read_size - 1] = 0;
    }
    req->obj_id = (uint64_t)g_quark_from_string(reader->line_buf);
  }
  return 0;
}
