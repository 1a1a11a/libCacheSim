//
// Created by Juncheng Yang on 5/7/20.
//


#include <math.h>
#include "include/readerUtils.h"
#include "../include/mimircache/reader.h"


#ifdef __cplusplus
extern "C"
{
#endif


void read_first_req(reader_t* reader, request_t* req){
  reset_reader(reader);
  read_one_req(reader, req);
  reset_reader(reader);
}


void read_last_req(reader_t* reader, request_t* req){
  request_t *temp_req = new_request(reader->base->obj_id_type);
  reset_reader(reader);
  reader->base->mmap_offset = reader->base->file_size - reader->base->item_size;

  if (reader->base->trace_type == CSV_TRACE || reader->base->trace_type == PLAIN_TXT_TRACE){
    reader->base->mmap_offset -= MAX_LINE_LEN * 2;
    fseek(reader->base->file, (long) reader->base->file_size - MAX_LINE_LEN*2, SEEK_SET);
    char *line_end;
    size_t line_len;
    gboolean end = find_line_ending(reader, &line_end, &line_len);
    reader->base->mmap_offset = (char *) line_end - reader->base->mapped_file;
  }

  read_one_req(reader, temp_req);
  while (req->valid){
    copy_request(req, temp_req);
    read_one_req(reader, temp_req);
  }
  req->valid = TRUE;
  reset_reader(reader);
  free_request(temp_req);
}



gint64 *get_window_boundary(reader_t *reader, guint32 window, gint32* n_window) {
  gint64 start_ts, end_ts, prev_ts, n_req = 0;
  request_t* req = new_request(reader->base->obj_id_type);
  read_first_req(reader, req);
  start_ts = req->real_time;
  read_last_req(reader, req);
  end_ts = req->real_time;

  *n_window = (gint64) floor((double) (end_ts - start_ts) / window);
  gint64 *boundary = g_new0(gint64, *n_window);

  prev_ts = start_ts;
  gint64 idx = 0;
  read_one_req(reader, req);

  n_req += 1;
  while (req->valid){
    if (req->real_time >= prev_ts + window){
      boundary[idx] = n_req;
      idx += (req->real_time - prev_ts) / window;
      prev_ts = req->real_time;
//      printf("%ld (%d) set %d %ld s %ld e %ld\n", req->real_time, (req->real_time - start_ts) / window, idx-1, n_req, start_ts, end_ts);
    }
    read_one_req(reader, req);
    n_req += 1;
  }

  if (idx == 0){
    ERROR("no request found\n");
    abort();
  }
  if (boundary[idx-1] != n_req)
    boundary[idx-1] = n_req;

  // it is possible that last is 0 due to second to last does not start a new one
  if (boundary[*n_window-1] == 0){
    *n_window = *n_window - 1;
    gint64 *resized_boundary = g_new0(gint64, *n_window);
    memcpy(resized_boundary, boundary, sizeof(gint64)*(*n_window));
    g_free(boundary);
    boundary = resized_boundary;
  }

  reset_reader(reader);
  return boundary;
}


#ifdef __cplusplus
}
#endif
