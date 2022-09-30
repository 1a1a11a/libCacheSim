
#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>

#include "readerInternal.h"

int txt_read_one_req(reader_t *const reader, request_t *const req) {
  int read_size =
      getline((char **)&reader->line_buf, &reader->line_buf_size, reader->file);
  if (read_size == -1) {
    req->valid = FALSE;
    return 1;
  }
  if (reader->obj_id_type == OBJ_ID_NUM) {
    char *end;
    req->obj_id = strtoull(reader->line_buf, &end, 0);
    if (req->obj_id == 0 && end == reader->line_buf) {
      ERROR("invalid object id: %s\n", reader->line_buf);
      abort();
    }
  } else {
    if (reader->line_buf[read_size - 1] == '\n')
      reader->line_buf[read_size - 1] = 0;
    req->obj_id = (uint64_t)g_quark_from_string(reader->line_buf);
  }
}

