//
//  reader.c
//  libCacheSim
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "customizedReader/oracleTwrBin.h"
#include "customizedReader/oracleBin.h"
#include "customizedReader/twrBin.h"
#include "include/binary.h"
#include "include/csv.h"
#include "include/libcsv.h"
#include "include/readerInternal.h"
#include "include/vscsi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_TAB 0x09
#define FILE_SPACE 0x20
#define FILE_CR 0x0d
#define FILE_LF 0x0a
#define FILE_COMMA 0x2c
#define FILE_QUOTE 0x22

reader_t *setup_reader(const char *const trace_path,
                       const trace_type_e trace_type,
                       const obj_id_type_e obj_id_type,
                       const reader_init_param_t *const reader_init_param) {

  int fd;
  struct stat st;
  reader_t *const reader = g_new0(reader_t, 1);

  reader->obj_id_type = obj_id_type;
  reader->trace_type = trace_type;
  reader->n_total_req = 0;
  reader->n_uniq_obj = 0;
  reader->n_read_req = 0;
  reader->mmap_offset = 0;
  reader->cloned = false;
  if (reader_init_param != NULL) {
    memcpy(&reader->init_params, reader_init_param,
           sizeof(reader_init_param_t));
  }

  if (strlen(trace_path) > MAX_FILE_PATH_LEN - 1) {
    ERROR("file name/path is too long(>%d), please use a shorter name\n",
          MAX_FILE_PATH_LEN);
    exit(1);
  } else {
    strcpy(reader->trace_path, trace_path);
  }

  // set up mmap region
  if ((fd = open(trace_path, O_RDONLY)) < 0) {
    ERROR("Unable to open '%s', %s\n", trace_path, strerror(errno));
    exit(1);
  }

  if ((fstat(fd, &st)) < 0) {
    close(fd);
    ERROR("Unable to fstat '%s', %s\n", trace_path, strerror(errno));
    exit(1);
  }
  reader->file_size = st.st_size;

  reader->mapped_file = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
#ifdef USE_HUGEPAGE
  madvise(reader->mapped_file, st.st_size, MADV_HUGEPAGE | MADV_SEQUENTIAL);
#endif

  if ((reader->mapped_file) == MAP_FAILED) {
    close(fd);
    reader->mapped_file = NULL;
    ERROR("Unable to allocate %llu bytes of memory, %s\n",
          (unsigned long long) st.st_size, strerror(errno));
    abort();
  }

  switch (trace_type) {
    case CSV_TRACE:
      csv_setup_reader(reader);
      break;
    case PLAIN_TXT_TRACE:
      reader->file = fopen(trace_path, "r");
      if (reader->file == 0) {
        ERROR("open trace file %s failed: %s\n", trace_path, strerror(errno));
        exit(1);
      }
      break;
    case VSCSI_TRACE:
      vscsi_setup(reader);
      break;
    case BIN_TRACE:
      binaryReader_setup(reader);
      break;
    case TWR_TRACE:
      twrReader_setup(reader);
      break;
    case ORACLE_TWR_TRACE:
      oracleTwr_setup(reader);
      break;
    case ORACLE_BIN_TRACE:
      oracleBin_setup(reader);
      break;
    default:
      ERROR("cannot recognize trace type: %c\n", reader->trace_type);
      exit(1);
  }
  close(fd);
  return reader;
}

int read_one_req(reader_t *const reader, request_t *const req) {
  reader->n_read_req += 1;
  req->n_req += 1;
  req->hv = 0;
  req->valid = true;
  char *line_end = NULL;
  size_t line_len;
  switch (reader->trace_type) {
    case CSV_TRACE: return csv_read_one_element(reader, req);
    case PLAIN_TXT_TRACE:
      if (reader->mmap_offset >= reader->file_size - 1) {
        req->valid = false;
        return 1;
      }
      char obj_id_str[MAX_OBJ_ID_LEN];
      find_line_ending(reader, &line_end, &line_len);
      if (reader->obj_id_type == OBJ_ID_NUM) {
        req->obj_id_int = str_to_obj_id(
            (char *) (reader->mapped_file + reader->mmap_offset), line_len);
      } else {
        if (line_len >= MAX_OBJ_ID_LEN) {
          line_len = MAX_OBJ_ID_LEN - 1;
          ERROR("obj_id len %zu larger than MAX_OBJ_ID_LEN %d\n",
                line_len, MAX_OBJ_ID_LEN);
          abort();
        }
        memcpy(obj_id_str, reader->mapped_file + reader->mmap_offset, line_len);
        obj_id_str[line_len] = 0;
        req->obj_id_int = (uint64_t) g_quark_from_string(obj_id_str);
      }
      reader->mmap_offset = (char *) line_end - reader->mapped_file;
      return 0;
    case VSCSI_TRACE: return vscsi_read_one_req(reader, req);
    case BIN_TRACE: return binary_read_one_req(reader, req);
    case TWR_TRACE: return twr_read_one_req(reader, req);
    case ORACLE_TWR_TRACE: return oracleTwr_read_one_req(reader, req);
    case ORACLE_BIN_TRACE: return oracleBin_read_one_req(reader, req);
    default:
      ERROR(
          "cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
          reader->trace_type);
      exit(1);
  }
}

int go_back_one_line(reader_t *const reader) {
  /* go back one request in the data
   return 0 on successful, non-zero otherwise
   */
  switch (reader->trace_type) {
    case CSV_TRACE:
    case PLAIN_TXT_TRACE:
      if (reader->mmap_offset == 0)
        return 1;

      // use last record size to save loop
      const char *cp = reader->mapped_file + reader->mmap_offset;
      if (reader->item_size) {
        // fix from LZ
        cp -= reader->item_size + 1;
        //                cp -= reader->item_size - 1;
      } else {
        /*  no record size, can only happen when it is the last line
         *  or when it is called in go_back_two_lines
         */
        cp--;
        // find current line ending
        // need to change to use memchr
        while (*cp == FILE_LF || *cp == FILE_CR) {
          cp--;
          if ((char *) cp < reader->mapped_file)
            return 1;
        }
      }
      /** now cp should point to either the last letter/non-LFCR of current line
       *  or points to somewhere after the beginning of current line beginning
       *  find the first character of current line
       */
      while ((char *) cp > reader->mapped_file && *cp != FILE_LF && *cp != FILE_CR) {
        cp--;
      }
      if ((void *) cp != reader->mapped_file)
        cp++;// jump over LFCR

      if ((char *) cp < reader->mapped_file) {
        ERROR("current pointer points before mapped file\n");
        exit(1);
      }
      // now cp points to the pos after LFCR before the line that should be read
      reader->mmap_offset = (char *) cp - reader->mapped_file;

      return 0;

    case BIN_TRACE:
    case TWR_TRACE:
    case ORACLE_TWR_TRACE:
    case ORACLE_BIN_TRACE:
    case VSCSI_TRACE:
      if (reader->mmap_offset >= reader->item_size)
        reader->mmap_offset -= (reader->item_size);
      else
        return -1;
      return 0;

    default:
      ERROR(
          "cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
          reader->trace_type);
      exit(1);
  }
}

int go_back_two_lines(reader_t *const reader) {
  /* go back two requests
   return 0 on successful, non-zero otherwise
   */
  switch (reader->trace_type) {
    case CSV_TRACE:
    case PLAIN_TXT_TRACE:
      if (go_back_one_line(reader) == 0) {
        reader->item_size = 0;
        return go_back_one_line(reader);
      } else
        return 1;
    case TWR_TRACE:
    case ORACLE_TWR_TRACE:
    case BIN_TRACE:
    case ORACLE_BIN_TRACE:
    case VSCSI_TRACE:
      if (reader->mmap_offset >= (reader->item_size * 2))
        reader->mmap_offset -= (reader->item_size) * 2;
      else
        return -1;
      return 0;
    default:
      ERROR(
          "cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
          reader->trace_type);
      exit(1);
  }
}

int read_one_req_above(reader_t *const reader, request_t *c) {
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
    return read_one_req(reader, c);
  else {
    c->valid = false;
    return 1;
  }
}

uint64_t skip_n_req(reader_t *const reader, const uint64_t N) {
  /* skip the next following N elements,
   Return value: the number of elements that are actually skipped,
   this will differ from N when it reaches the end of file
   */
  uint64_t count = N;

  switch (reader->trace_type) {
    case CSV_TRACE: csv_skip_N_elements(reader, N);
    case PLAIN_TXT_TRACE:;
      char *line_end = NULL;
      uint64_t i;
      bool end = false;
      size_t line_len;
      for (i = 0; i < N; i++) {
        end = find_line_ending(reader, &line_end, &line_len);
        reader->mmap_offset = (char *) line_end - reader->mapped_file;
        if (end) {
          if (reader->trace_type == CSV_TRACE) {
            csv_params_t *params = reader->reader_params;
            params->reader_end = true;
          }
          count = i + 1;
          break;
        }
      }

      break;
    case TWR_TRACE:
    case ORACLE_TWR_TRACE:
    case BIN_TRACE:
    case ORACLE_BIN_TRACE:
    case VSCSI_TRACE:
      if (reader->mmap_offset + N * reader->item_size <= reader->file_size) {
        reader->mmap_offset = reader->mmap_offset + N * reader->item_size;
      } else {
        count = (uint64_t)((reader->file_size - reader->mmap_offset) / reader->item_size);
        reader->mmap_offset = reader->file_size;
        WARNING("required to skip %lu requests, but only %lu requests left\n",
                (unsigned long) N, (unsigned long) count);
      }

      break;
    default:
      ERROR("cannot recognize reader trace type: %c\n",
            reader->trace_type);
      exit(1);
      break;
  }
  return count;
}

void reset_reader(reader_t *const reader) {
  /* rewind the reader back to beginning */
  reader->mmap_offset = 0;
  switch (reader->trace_type) {
    case CSV_TRACE: csv_reset_reader(reader);
    case PLAIN_TXT_TRACE:
    case TWR_TRACE:
    case ORACLE_TWR_TRACE:
    case BIN_TRACE:
    case ORACLE_BIN_TRACE:
    case VSCSI_TRACE: break;
    default:
      ERROR(
          "cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
          reader->trace_type);
      exit(1);
      break;
  }
}

uint64_t get_num_of_req(reader_t *const reader) {
  if (reader->n_total_req > 0)
    return reader->n_total_req;

  uint64_t old_offset = reader->mmap_offset;
  reader->mmap_offset = 0;
  uint64_t n_req = 0;
  // why can't reset here?
  // reset_reader(reader);

  switch (reader->trace_type) {
    case CSV_TRACE:
      /* same as plain text, except when has_header, it needs to reduce by 1  */
    case PLAIN_TXT_TRACE:;
      char *line_end = NULL;
      size_t line_len;
      while (!find_line_ending(reader, &line_end, &line_len)) {
        reader->mmap_offset = (char *) line_end - reader->mapped_file;
        n_req++;
      }
      n_req++;
      if (reader->trace_type == CSV_TRACE)
        if (((csv_params_t *) (reader->reader_params))->has_header)
          n_req--;
      break;
    case TWR_TRACE:
    case ORACLE_TWR_TRACE:
    case BIN_TRACE:
    case ORACLE_BIN_TRACE:
    case VSCSI_TRACE: return reader->n_total_req;
    default:
      ERROR(
          "cannot recognize reader trace type, given reader trace_type: %c\n",
          reader->trace_type);
      exit(1);
      break;
  }
  reader->n_total_req = n_req;
  reader->mmap_offset = old_offset;
  return n_req;
}

reader_t *clone_reader(const reader_t *const reader_in) {
  reader_t *const reader =
      setup_reader(reader_in->trace_path, reader_in->trace_type,
                   reader_in->obj_id_type, &reader_in->init_params);
  reader->n_total_req = reader_in->n_total_req;

  munmap(reader->mapped_file, reader->file_size);
  reader->mapped_file = reader_in->mapped_file;
  reader->cloned = true;
  return reader;
}

int close_reader(reader_t *const reader) {
  /* close the file in the reader or unmmap the memory in the file
   then free the memory of reader object
   Return value: Upon successful completion 0 is returned.
   Otherwise, EOF is returned and the global variable errno is set to
   indicate the error.  In either case no further
   access to the stream is possible.*/

  switch (reader->trace_type) {
    case CSV_TRACE:;
      csv_params_t *params = reader->reader_params;
      fclose(reader->file);
      csv_free(params->csv_parser);
      g_free(params->csv_parser);
      break;
    case PLAIN_TXT_TRACE:
      fclose(reader->file);
      break;
    case TWR_TRACE:
    case ORACLE_TWR_TRACE:
    case BIN_TRACE:
    case ORACLE_BIN_TRACE:
    case VSCSI_TRACE: break;
    default:
      ERROR(
          "cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
          reader->obj_id_type);
      abort();
  }

  if (!reader->cloned)
    munmap(reader->mapped_file, reader->file_size);
  if (reader->reader_params)
    g_free(reader->reader_params);
  g_free(reader);
  return 0;
}

void set_no_eof(reader_t *const reader) {
  // remove eof flag for reader
  switch (reader->trace_type) {
    case CSV_TRACE:
      csv_set_no_eof(reader);
      break;
    case PLAIN_TXT_TRACE:
    case TWR_TRACE:
    case ORACLE_TWR_TRACE:
    case BIN_TRACE:
    case ORACLE_BIN_TRACE:
    case VSCSI_TRACE: break;
    default:
      ERROR(
          "cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
          reader->trace_type);
      exit(1);
      break;
  }
}

void reader_set_read_pos(reader_t *const reader, const double pos) {
  /* jason (202004): this may not work for CSV
   */

  /* jump to given postion, like 1/3, or 1/2 and so on
   * reference number will NOT change in the function! .
   * due to above property, this function is dangerous to use.
   */
  reader->mmap_offset = (long) (reader->file_size * pos);
  if (reader->trace_type == CSV_TRACE
      || reader->trace_type == PLAIN_TXT_TRACE) {
    reader->item_size = 0;
    /* for plain and csv file, if it points to the end, we need to rewind by 1,
     * because mapped_file+file_size-1 is the last byte
     */
    if ((pos > 1 && pos - 1 < 0.0001) || (pos < 1 && 1 - pos < 0.0001))
      reader->mmap_offset--;
  }
}

/**
 *  find the closest end of line, store in line_end
 *  line_end should point to the first character of next line (character that is
 * not current line), in other words, the character after all LFCR line_len is
 * the length of current line, does not include CRLF, nor \0 return true, if end
 * of file return false else *
 * @param reader
 * @param line_end
 * @param line_len
 * @return
 */
bool find_line_ending(reader_t *const reader, char **line_end,
                      size_t *const line_len) {
  /**
   *  find the closest line ending, save in line_end
   *  line_end should point to the character that is not current line,
   *  in other words, the character after all LFCR
   *  line_len is the length of current line, does not include CRLF, nor \0
   *  return true, if end of file
   *  return false else
   */

  size_t size = MAX_LINE_LEN;
  *line_end = NULL;

  while (*line_end == NULL) {
    if (size > (long) reader->file_size - reader->mmap_offset)
      size = reader->file_size - reader->mmap_offset;
    *line_end = (char *) memchr(
        (void *) ((char *) (reader->mapped_file) + reader->mmap_offset), CSV_LF,
        size);
    if (*line_end == NULL)
      *line_end = (char *) memchr(
          (char *) (reader->mapped_file) + reader->mmap_offset, CSV_CR, size);

    if (*line_end == NULL) {
      if (size == MAX_LINE_LEN) {
        WARNING("line length exceeds %d characters, now double max_line_len\n",
                MAX_LINE_LEN);
        size *= 2;
      } else {
        /*  end of trace, does not -1 here
         *  if file ending has no CRLF, then file_end points to end of file,
         * return true; if file ending has one or more CRLF, it will goes to
         * next while, then file_end points to end of file, still return true
         */
        *line_end = (char *) (reader->mapped_file) + reader->file_size;
        *line_len = size;
        reader->item_size = *line_len;
        return true;
      }
    }
  }
  // currently line_end points to LFCR
  *line_len =
      *line_end - ((char *) (reader->mapped_file) + reader->mmap_offset);

  while ((long) (*line_end - (char *) (reader->mapped_file)) < (long) (reader->file_size) - 1 && (*(*line_end + 1) == CSV_CR || *(*line_end + 1) == CSV_LF || *(*line_end + 1) == CSV_TAB || *(*line_end + 1) == CSV_SPACE)) {
    (*line_end)++;
    if ((long) (*line_end - (char *) (reader->mapped_file)) == (long) (reader->file_size) - 1) {
      reader->item_size = *line_len;
      return true;
    }
  }
  if ((long) (*line_end - (char *) (reader->mapped_file)) == (long) (reader->file_size) - 1) {
    reader->item_size = *line_len;
    return true;
  }
  // move to next line, non LFCR
  (*line_end)++;
  reader->item_size = *line_len;

  return false;
}

#ifdef __cplusplus
}
#endif
