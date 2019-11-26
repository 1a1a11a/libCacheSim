//
//  reader.c
//  mimircache
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//


#include "../../include/mimircache/reader.h"
#include "../../include/mimircache/vscsiReader.h"
#include "../../include/mimircache/binaryReader.h"
#include "../../include/mimircache/csvReader.h"
#include "../../include/mimircache/logging.h"
#include "libcsv.h"

/* when obj_id/LBA is number, we plus one to it, to avoid cases of block 0 */

/* special case in ascii reader
 * multiple blank line in the middle
 * each line has only one character
 * multiple or non blank line at the end of file
 * csv header when go back one line
 * ending in go back one line
 */



#ifdef __cplusplus
extern "C"
{
#endif


reader_t *setup_reader(const char *const file_loc,
                       const trace_type_t trace_type,
                       const obj_id_t obj_id_type,
                       const void *const setup_params) {
  /* setup the reader struct for reading trace
   trace_type: CSV_TRACE, PLAIN_TXT_TRACE, BIN_TRACE, VSCSI_TRACE
   obj_id_type: OBJ_ID_NUM, OBJ_ID_STR
   Return value: a pointer to READER struct, the returned reader
   needs to be explicitly closed by calling close_reader */

  int fd;
  struct stat st;
  reader_t *const reader = g_new0(reader_t, 1);
  reader->base = g_new0(reader_base_t, 1);
  reader->sdata = g_new0(reader_data_share_t, 1);
  reader->udata = g_new0(reader_data_unique_t, 1);

  reader->sdata->break_points = NULL;
  reader->sdata->last_access = NULL;
  reader->sdata->reuse_dist = NULL;
  reader->sdata->max_reuse_dist = 0;

  reader->udata->hit_ratio = NULL;
  reader->udata->hit_ratio_shards = NULL;

  reader->base->n_total_req = -1;
  reader->base->obj_id_type = obj_id_type;
  reader->base->trace_type = trace_type;
  reader->base->init_params = NULL;
  reader->base->mmap_offset = 0;


  if (strlen(file_loc) > MAX_FILE_PATH_LEN - 1) {
    ERROR("file name/path is too long(>%d), "
          "please use a shorter name\n", MAX_FILE_PATH_LEN);
    exit(1);
  } else {
    strcpy(reader->base->trace_path, file_loc);
  }


  // set up mmap region
  if ((fd = open(file_loc, O_RDONLY)) < 0) {
    ERROR("Unable to open '%s', %s\n", file_loc, strerror(errno));
    exit(1);
  }

  if ((fstat(fd, &st)) < 0) {
    close(fd);
    ERROR("Unable to fstat '%s', %s\n", file_loc, strerror(errno));
    exit(1);
  }

  if ((reader->base->mapped_file = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
    close(fd);
    reader->base->mapped_file = NULL;
    ERROR("Unable to allocate %llu bytes of memory, %s\n", (unsigned long long) st.st_size,
          strerror(errno));
    abort();
  }

  reader->base->file_size = st.st_size;

  switch (trace_type) {
    case CSV_TRACE:
      csv_setup_Reader(file_loc, reader, setup_params);
      break;
    case PLAIN_TXT_TRACE:
      reader->base->file = fopen(file_loc, "r");
      if (reader->base->file == 0) {
        ERROR("open trace file %s failed: %s\n", file_loc, strerror(errno));
        exit(1);
      }
      break;
    case VSCSI_TRACE:
      vscsi_setup(file_loc, reader);
      break;
    case BIN_TRACE:
      binaryReader_setup(file_loc, reader, setup_params);
      break;
    default: ERROR("cannot recognize reader trace type, given reader trace type: %c\n",
                   reader->base->trace_type);
      exit(1);
      break;
  }
  close(fd);
//    DEBUG("init reader trace type %c obj_id_type %c\n", reader->base->trace_type, reader->base->obj_id_type);
  return reader;
}

void read_one_req(reader_t *const reader, request_t *const req) {
  /* read one request from reader,
   and store it in the pre-allocated request_t req
   */
//    req->ts ++;
  char *line_end = NULL;
  size_t line_len;
  switch (reader->base->trace_type) {
    case CSV_TRACE:
      csv_read_one_element(reader, req);
      break;
    case PLAIN_TXT_TRACE:
      if (reader->base->mmap_offset >= reader->base->file_size - 1) {
        req->valid = FALSE;
        break;
      }
      find_line_ending(reader, &line_end, &line_len);
      if (reader->base->obj_id_type == OBJ_ID_NUM) {
        req->obj_id_ptr = GSIZE_TO_POINTER(str_to_gsize(
          (char *) (reader->base->mapped_file + reader->base->mmap_offset), line_len));
      } else {
        memcpy(req->obj_id_ptr, reader->base->mapped_file + reader->base->mmap_offset, line_len);
        ((char *) (req->obj_id_ptr))[line_len] = 0;
      }
      reader->base->mmap_offset = (char *) line_end - reader->base->mapped_file;
      break;
    case VSCSI_TRACE:
      vscsi_read(reader, req);
      break;
    case BIN_TRACE:
      binary_read(reader, req);
      break;
    default: ERROR("cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
                   reader->base->trace_type);
      exit(1);
      break;
  }
}

int go_back_one_line(reader_t *const reader) {
  /* go back one request in the data
   return 0 on successful, non-zero otherwise
   */
  switch (reader->base->trace_type) {
    case CSV_TRACE:
    case PLAIN_TXT_TRACE:
      if (reader->base->mmap_offset == 0)
        return 1;

      // use last record size to save loop
      const char *cp = reader->base->mapped_file + reader->base->mmap_offset;
      if (reader->base->item_size) {
        // fix from LZ
        cp -= reader->base->item_size + 1;
//                cp -= reader->base->item_size - 1;
      } else {
        /*  no record size, can only happen when it is the last line
         *  or when it is called in go_back_two_lines
         */
        cp--;
        // find current line ending
        // need to change to use memchr
        while (*cp == FILE_LF || *cp == FILE_CR) {
          cp--;
          if ((char *) cp < reader->base->mapped_file)
            return 1;
        }
      }
      /** now cp should point to either the last letter/non-LFCR of current line
       *  or points to somewhere after the beginning of current line beginning
       *  find the first character of current line
       */
      while ((char *) cp > reader->base->mapped_file &&
             *cp != FILE_LF && *cp != FILE_CR) {
        cp--;
      }
      if ((void *) cp != reader->base->mapped_file)
        cp++; // jump over LFCR

      if ((char *) cp < reader->base->mapped_file) {
        ERROR("current pointer points before mapped file\n");
        exit(1);
      }
      // now cp points to the pos after LFCR before the line that should be read
      reader->base->mmap_offset = (char *) cp - reader->base->mapped_file;

      return 0;

    case BIN_TRACE:
    case VSCSI_TRACE:
      if (reader->base->mmap_offset >= reader->base->item_size)
        reader->base->mmap_offset -= (reader->base->item_size);
      else
        return -1;
      return 0;

    default: ERROR("cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
                   reader->base->trace_type);
      exit(1);
  }
}


int go_back_two_lines(reader_t *const reader) {
  /* go back two requests
   return 0 on successful, non-zero otherwise
   */
  switch (reader->base->trace_type) {
    case CSV_TRACE:
    case PLAIN_TXT_TRACE:
      if (go_back_one_line(reader) == 0) {
        reader->base->item_size = 0;
        return go_back_one_line(reader);
      } else
        return 1;
    case BIN_TRACE:
    case VSCSI_TRACE:
      if (reader->base->mmap_offset >= (reader->base->item_size * 2))
        reader->base->mmap_offset -= (reader->base->item_size) * 2;
      else
        return -1;
      return 0;
    default: ERROR("cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
                   reader->base->trace_type);
      exit(1);
  }
}


void read_one_req_above(reader_t *const reader, request_t *c) {
  /* read one request from reader precede current position,
   * in other words, read the line above current line,
   * and currently file points to either the end of current line or
   * beginning of next line.
   then store it in the pre-allocated request_t c, current given
   size for the element(obj_id) is 128 bytes(obj_id_label_size).
   after reading the new position is at the beginning of readed request
   in other words, this method is called for reading from end to beginngng
   */
  if (go_back_two_lines(reader) == 0)
    read_one_req(reader, c);
  else {
    c->valid = FALSE;
  }


}


guint64 skip_N_elements(reader_t *const reader, const guint64 N) {
  /* skip the next following N elements,
   Return value: the number of elements that are actually skipped,
   this will differ from N when it reaches the end of file
   */
  guint64 count = N;

  switch (reader->base->trace_type) {
    case CSV_TRACE:
      csv_skip_N_elements(reader, N);
    case PLAIN_TXT_TRACE:;
      char *line_end = NULL;
      guint64 i;
      gboolean end = FALSE;
      size_t line_len;
      for (i = 0; i < N; i++) {
        end = find_line_ending(reader, &line_end, &line_len);
        reader->base->mmap_offset = (char *) line_end - reader->base->mapped_file;
        if (end) {
          if (reader->base->trace_type == CSV_TRACE) {
            csv_params_t *params = reader->reader_params;
            params->reader_end = TRUE;
          }
          count = i + 1;
          break;
        }
      }

      break;
    case BIN_TRACE:
    case VSCSI_TRACE:
      if (reader->base->mmap_offset + N * reader->base->item_size
          <= reader->base->file_size) {
        reader->base->mmap_offset = reader->base->mmap_offset + N * reader->base->item_size;
      } else {
        count = (guint64) ((reader->base->file_size - reader->base->mmap_offset)
                           / reader->base->item_size);
        reader->base->mmap_offset = reader->base->file_size;
        WARNING("required to skip %lu requests, but only %lu requests left\n",
                (unsigned long) N, (unsigned long) count);
      }

      break;
    default: ERROR("cannot recognize reader trace type: %c\n", reader->base->trace_type);
      exit(1);
      break;
  }
  return count;
}

void reset_reader(reader_t *const reader) {
  /* rewind the reader back to beginning */
  reader->base->mmap_offset = 0;
  switch (reader->base->trace_type) {
    case CSV_TRACE:
      csv_reset_reader(reader);
    case PLAIN_TXT_TRACE:
    case BIN_TRACE:
    case VSCSI_TRACE:
      break;
    default: ERROR("cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
                   reader->base->trace_type);
      exit(1);
      break;
  }
}


void reader_set_read_pos(reader_t *const reader, const double pos) {
  /* jump to given postion, like 1/3, or 1/2 and so on
   * reference number will NOT change in the function! .
   * due to above property, this function is deemed as deprecated.
   */
  reader->base->mmap_offset = (long) (reader->base->file_size * pos);
  if (reader->base->trace_type == CSV_TRACE || reader->base->trace_type == PLAIN_TXT_TRACE) {
    reader->base->item_size = 0;
    /* for plain and csv file, if it points to the end, we need to rewind by 1,
     * because mapped_file+file_size-1 is the last byte
     */
    if ((pos > 1 && pos - 1 < 0.0001) || (pos < 1 && 1 - pos < 0.0001))
      reader->base->mmap_offset--;
  }
}


guint64 get_num_of_req(reader_t *const reader) {
  if (reader->base->n_total_req != 0 && reader->base->n_total_req != -1)
    return reader->base->n_total_req;

  guint64 old_offset = reader->base->mmap_offset;
  reader->base->mmap_offset = 0;
  guint64 n_req = 0;
  // why can't reset here?
  // reset_reader(reader);

  switch (reader->base->trace_type) {
    case CSV_TRACE:
      /* same as plain text, except when has_header, it needs to reduce by 1  */
    case PLAIN_TXT_TRACE:;
      char *line_end = NULL;
      size_t line_len;
      while (!find_line_ending(reader, &line_end, &line_len)) {
        reader->base->mmap_offset = (char *) line_end - reader->base->mapped_file;
        n_req++;
      }
      n_req++;
      if (reader->base->trace_type == CSV_TRACE)
        if (((csv_params_t *) (reader->reader_params))->has_header)
          n_req--;
      break;
    case BIN_TRACE:
    case VSCSI_TRACE:
      return reader->base->n_total_req;
    default: ERROR("cannot recognize reader trace type, given reader trace_type: %c\n",
                   reader->base->trace_type);
      exit(1);
      break;
  }
  reader->base->n_total_req = n_req;
  reader->base->mmap_offset = old_offset;
  return n_req;
}


reader_t *clone_reader(reader_t *const reader_in) {
  /* this function clone the given reader to give an exactly same reader,
   * note that 20191120, the reader cloned by this function does not preserve
   * the mmap_offset in the file as original reader */

  reader_t *const reader = setup_reader(reader_in->base->trace_path,
                                        reader_in->base->trace_type,
                                        reader_in->base->obj_id_type,
                                        reader_in->base->init_params);
  memcpy(reader->sdata, reader_in->sdata, sizeof(reader_data_share_t));

  // this is not ideal, but we don't want to multiple mapped files
  // Jason: why?
//    munmap (reader->base->mapped_file, reader->base->file_size);
//    reader->base->mapped_file = reader_in->base->mapped_file;
//    reader->base->mmap_offset = reader_in->base->mmap_offset;
  reader->base->n_total_req = reader_in->base->n_total_req;

//    if (reader->base->trace_type == CSV_TRACE){
//        csv_params_t* params = reader->reader_params;
//        csv_params_t* params_in = reader_in->reader_params;
//
//        fseek(reader->base->file, ftell(reader_in->base->file), SEEK_SET);
//        memcpy(params->csv_parser, params_in->csv_parser, sizeof(struct csv_parser));
//    }
  return reader;
}


int close_reader(reader_t *const reader) {
  /* close the file in the reader or unmmap the memory in the file
   then free the memory of reader object
   Return value: Upon successful completion 0 is returned.
   Otherwise, EOF is returned and the global variable errno is set to
   indicate the error.  In either case no further
   access to the stream is possible.*/

  switch (reader->base->trace_type) {
    case CSV_TRACE:;
      csv_params_t *params = reader->reader_params;
      fclose(reader->base->file);
      csv_free(params->csv_parser);
      g_free(params->csv_parser);
      break;
    case PLAIN_TXT_TRACE:
      fclose(reader->base->file);
      break;
    case BIN_TRACE:
    case VSCSI_TRACE:
      break;
    default: ERROR("cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
                   reader->base->trace_type);
  }

  munmap(reader->base->mapped_file, reader->base->file_size);
  if (reader->base->init_params)
    g_free(reader->base->init_params);

  if (reader->reader_params)
    g_free(reader->reader_params);

  if (reader->sdata) {
    if (reader->sdata->last_access) {
      g_free(reader->sdata->last_access);
    }

    if (reader->sdata->reuse_dist) {
      g_free(reader->sdata->reuse_dist);
    }

    if (reader->sdata->break_points) {
      g_array_free(reader->sdata->break_points->array, TRUE);
      g_free(reader->sdata->break_points);
    }
  }
  if (reader->udata) {
    if (reader->udata->hit_ratio)
      g_free(reader->udata->hit_ratio);
    if (reader->udata->hit_ratio_shards)
      g_free(reader->udata->hit_ratio_shards);
  }

  g_free(reader->base);
  g_free(reader->udata);
  g_free(reader->sdata);
  g_free(reader);
  return 0;
}


int close_reader_unique(reader_t *const reader) {
  /* close the file in the reader or unmmap the memory in the file
   then free the memory of reader object
   Return value: Upon successful completion 0 is returned.
   Otherwise, EOF is returned and the global variable errno is set to
   indicate the error.  In either case no further
   access to the stream is possible.*/

  switch (reader->base->trace_type) {
    case CSV_TRACE:;
      csv_params_t *params = reader->reader_params;
      fclose(reader->base->file);
      csv_free(params->csv_parser);
      g_free(params->csv_parser);
      break;
    case PLAIN_TXT_TRACE:
      fclose(reader->base->file);
      break;
    case BIN_TRACE:
    case VSCSI_TRACE:
      break;
    default: ERROR("cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
                   reader->base->trace_type);
  }

  if (reader->base->init_params)
    g_free(reader->base->init_params);

  if (reader->reader_params)
    g_free(reader->reader_params);

  if (reader->udata) {
    if (reader->udata->hit_ratio)
      g_free(reader->udata->hit_ratio);
    if (reader->udata->hit_ratio_shards)
      g_free(reader->udata->hit_ratio_shards);
  }

  g_free(reader->base);
  g_free(reader->sdata);
  g_free(reader->udata);

  g_free(reader);
  return 0;
}



// not supported

int read_one_request_all_info(reader_t *const reader, void *storage) {
  /* read one request from reader,
   and store it in the pre-allocated memory pointed at storage.
   return 1 when finished or error, otherwise, return 0.
   */
  switch (reader->base->trace_type) {
    case CSV_TRACE:
      printf("currently c reader is not supported yet\n");
      exit(1);
      break;
    case PLAIN_TXT_TRACE:
      if (fscanf(reader->base->file, "%s", (char *) storage) == EOF)
        return 1;
      else {
        if (strlen((char *) storage) == 2 && ((char *) storage)[1] == '\0'
            && (((char *) storage)[0] == FILE_LF || ((char *) storage)[0] == FILE_CR))
          return read_one_request_all_info(reader, storage);
        return 0;
      }
      break;
    case BIN_TRACE:
    case VSCSI_TRACE:
      printf("currently v/b reader is not supported yet\n");
      exit(1);
      break;
    default: ERROR("cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
                   reader->base->trace_type);
      exit(1);
      break;
  }
  return 0;
}

void set_no_eof(reader_t *const reader) {
  // remove eof flag for reader
  switch (reader->base->trace_type) {
    case CSV_TRACE:
      csv_set_no_eof(reader);
      break;
    case PLAIN_TXT_TRACE:
    case BIN_TRACE:
    case VSCSI_TRACE:
      break;
    default: ERROR("cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
                   reader->base->trace_type);
      exit(1);
      break;
  }
}

gboolean find_line_ending(reader_t *const reader, char **line_end,
                          size_t *const line_len) {
  /**
   *  find the closest line ending, save at line_end
   *  line_end should point to the character that is not current line,
   *  in other words, the character after all LFCR
   *  line_len is the length of current line, does not include CRLF, nor \0
   *  return TRUE, if end of file
   *  return FALSE else
   */

  size_t size = MAX_LINE_LEN;
  *line_end = NULL;

  while (*line_end == NULL) {
    if (size > (long) reader->base->file_size - reader->base->mmap_offset)
      size = reader->base->file_size - reader->base->mmap_offset;
    *line_end = (char *) memchr(
      (void *) ((char *) (reader->base->mapped_file) + reader->base->mmap_offset),
      CSV_LF, size);
    if (*line_end == NULL)
      *line_end = (char *) memchr((char *) (reader->base->mapped_file) +
                                  reader->base->mmap_offset,
                                  CSV_CR, size);

    if (*line_end == NULL) {
      if (size == MAX_LINE_LEN) {
        WARNING("line length exceeds %d characters, now double max_line_len\n",
                MAX_LINE_LEN);
        size *= 2;
      } else {
        /*  end of trace, does not -1 here
         *  if file ending has no CRLF, then file_end points to end of file,
         * return TRUE; if file ending has one or more CRLF, it will goes to
         * next while, then file_end points to end of file, still return TRUE
         */
        *line_end =
          (char *) (reader->base->mapped_file) + reader->base->file_size;
        *line_len = size;
        reader->base->item_size = *line_len;
        return TRUE;
      }
    }
  }
  // currently line_end points to LFCR
  *line_len =
    *line_end - ((char *) (reader->base->mapped_file) + reader->base->mmap_offset);

  while ((long) (*line_end - (char *) (reader->base->mapped_file)) <
         (long) (reader->base->file_size) - 1 &&
         (*(*line_end + 1) == CSV_CR || *(*line_end + 1) == CSV_LF ||
          *(*line_end + 1) == CSV_TAB || *(*line_end + 1) == CSV_SPACE)) {
    (*line_end)++;
    if ((long) (*line_end - (char *) (reader->base->mapped_file)) ==
        (long) (reader->base->file_size) - 1) {
      reader->base->item_size = *line_len;
      return TRUE;
    }
  }
  if ((long) (*line_end - (char *) (reader->base->mapped_file)) ==
      (long) (reader->base->file_size) - 1) {
    reader->base->item_size = *line_len;
    return TRUE;
  }
  // move to next line, non LFCR
  (*line_end)++;
  reader->base->item_size = *line_len;

  return FALSE;
}


guint64 read_one_timestamp(reader_t *const reader) {
  return 0;
}


void read_one_op(reader_t *const reader, void *op) {
  ;
}

guint64 read_one_request_size(reader_t *const reader) {
  return 0;
}


#ifdef __cplusplus
}
#endif
