//
//  reader.c
//  libCacheSim
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "customizedReader/oracleTwrBin.h"
#include "customizedReader/oracleGeneralBin.h"
#include "customizedReader/oracleAkamaiBin.h"
#include "customizedReader/oracleWikiBin.h"

#include "customizedReader/twrBin.h"
#include "customizedReader/akamaiBin.h"
#include "customizedReader/standardBin.h"
#include "customizedReader/wikiBin.h"

#include "generalReader/binary.h"
#include "generalReader/csv.h"
#include "generalReader/libcsv.h"
#include "readerInternal.h"
#include "customizedReader/vscsi.h"

#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

reader_t *setup_reader(const char *const trace_path,
                       trace_type_e trace_type,
                       obj_id_type_e obj_id_type,
                       const reader_init_param_t *reader_init_param) {

  int fd;
  struct stat st;
  reader_t *const reader = g_new0(reader_t, 1);
  memset(reader, 0, sizeof(reader_t));

  reader->obj_id_type = obj_id_type;
  reader->trace_type = trace_type;
  reader->trace_format = INVALID_TRACE_FORMAT;
  reader->n_total_req = 0;
  reader->n_read_req = 0;
  reader->mmap_offset = 0;
  reader->item_size = 0;
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
#if defined(USE_HUGEPAGE) && !defined(__APPLE__)
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
  case CSV_TRACE:csv_setup_reader(reader);
    break;
  case PLAIN_TXT_TRACE:
    reader->trace_format = TXT_TRACE_FORMAT;
//    reader->file = fopen(trace_path, "r");
//    if (reader->file == 0) {
//      ERROR("open trace file %s failed: %s\n", trace_path, strerror(errno));
//      exit(1);
//    }
    break;
  case BIN_TRACE:binaryReader_setup(reader); break;
  case VSCSI_TRACE:vscsiReader_setup(reader); break;
  case TWR_BIN_TRACE:twrReader_setup(reader); break;
  case AKAMAI_BIN_TRACE:akamaiReader_setup(reader); break;
  case WIKI16u_BIN_TRACE: wiki2016uReader_setup(reader); break;
  case WIKI19u_BIN_TRACE: wiki2019uReader_setup(reader); break;
  case WIKI19t_BIN_TRACE: wiki2019tReader_setup(reader); break;
  case STANDARD_BIN_III_TRACE:standardBinIII_setup(reader); break;
  case STANDARD_BIN_IQI_TRACE:standardBinIQI_setup(reader); break;
  case ORACLE_GENERAL_BIN:oracleGeneralBin_setup(reader); break;
  case ORACLE_TWR_BIN:oracleTwrBin_setup(reader); break;
  case ORACLE_AKAMAI_BIN:oracleAkamai_setup(reader); break;
  case ORACLE_WIKI16u_BIN:oracleWiki2016uReader_setup(reader); break;
  case ORACLE_WIKI19u_BIN:oracleWiki2019uReader_setup(reader); break;
  case ORACLE_WIKI19t_BIN:oracleWiki2019tReader_setup(reader); break;
  default:ERROR("cannot recognize trace type: %c\n", reader->trace_type);
    exit(1);
  }

  if (reader->trace_type != PLAIN_TXT_TRACE && reader->trace_type != CSV_TRACE
      && reader->item_size != 0 && reader->file_size % reader->item_size != 0) {
    WARNING("trace file size %zu is not multiple of record size %zu\n",
            reader->file_size, reader->item_size);
  }

  if (reader->trace_format == INVALID_TRACE_FORMAT) {
    ERROR("trace reader setup did not set "
          "trace format (BINARY_TRACE_FORMAT/TXT_TRACE_FORMAT)\n");
    abort();
  }

  close(fd);
  return reader;
}

int read_one_req(reader_t *const reader, request_t *const req) {
  if (reader->mmap_offset >= reader->file_size - 1) {
    req->valid = FALSE;
    return 1;
  }

  int status;
  reader->n_read_req += 1;
  req->n_req += 1;
  req->hv = 0;
  req->valid = true;

  switch (reader->trace_type) {
  case CSV_TRACE: status = csv_read_one_element(reader, req); break;
  case PLAIN_TXT_TRACE:;
    char *line_end = NULL;
    size_t line_len;
    static __thread char obj_id_str[MAX_OBJ_ID_LEN];
    find_line_ending(reader, &line_end, &line_len);
    if (reader->obj_id_type == OBJ_ID_NUM) {
      req->obj_id_int = str_to_obj_id((reader->mapped_file + reader->mmap_offset), line_len);
    } else {
      if (line_len >= MAX_OBJ_ID_LEN) {
        ERROR("obj_id len %zu larger than MAX_OBJ_ID_LEN %d\n",
              line_len, MAX_OBJ_ID_LEN);
        abort();
      }
      memcpy(obj_id_str, reader->mapped_file + reader->mmap_offset, line_len);
      obj_id_str[line_len] = 0;
      req->obj_id_int = (uint64_t) g_quark_from_string(obj_id_str);
    }
    reader->mmap_offset = (char *) line_end - reader->mapped_file;
    status = 0;
    break;
  case BIN_TRACE: status = binary_read_one_req(reader, req); break;
  case VSCSI_TRACE: status = vscsi_read_one_req(reader, req); break;
  case TWR_BIN_TRACE: status = twr_read_one_req(reader, req); break;
  case AKAMAI_BIN_TRACE: status = akamai_read_one_req(reader, req); break;
  case WIKI16u_BIN_TRACE: status = wiki2016u_read_one_req(reader, req); break;
  case WIKI19u_BIN_TRACE: status = wiki2019u_read_one_req(reader, req); break;
  case WIKI19t_BIN_TRACE: status = wiki2019t_read_one_req(reader, req); break;
  case STANDARD_BIN_III_TRACE: status = standardBinIII_read_one_req(reader, req); break;
  case STANDARD_BIN_IQI_TRACE: status = standardBinIQI_read_one_req(reader, req); break;
  case ORACLE_GENERAL_BIN: status = oracleGeneralBin_read_one_req(reader, req); break;
  case ORACLE_TWR_BIN: status = oracleTwrBin_read_one_req(reader, req); break;
  case ORACLE_AKAMAI_BIN: status = oracleAkamai_read_one_req(reader, req); break;
  case ORACLE_WIKI16u_BIN: status = oracleWiki2016u_read_one_req(reader, req); break;
  case ORACLE_WIKI19u_BIN: status = oracleWiki2019u_read_one_req(reader, req); break;
  case ORACLE_WIKI19t_BIN: status = oracleWiki2019t_read_one_req(reader, req); break;
  default:ERROR(
        "cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
        reader->trace_type);
    exit(1);
  }
#if defined(ENABLE_TRACE_SAMPLING) && ENABLE_TRACE_SAMPLING == 1
  if (reader->sample != NULL) {
    if (!reader->sample(reader->sampler, req)) {
      /* skip this request and read next */
        return read_one_req(reader, req);
      }
  }
#endif

  return status;
}

/**
 * go back one request in the trace
 * @param reader
 * @return return 0 on successful, non-zero otherwise
 */
int go_back_one_line(reader_t *const reader) {
  switch (reader->trace_format) {
  case TXT_TRACE_FORMAT:
    if (reader->mmap_offset == 0)
      return 1;

    const char *cp = reader->mapped_file + reader->mmap_offset - 1;
    while (isspace(*cp)) {
      if (--cp < reader->mapped_file)
        return 1;
    }
    /** cp now points to either the last non-CRLF of current line */
    while (cp >= reader->mapped_file && !isspace(*cp)) {
      cp--;
    }
    /* cp now points to CRLF of last line or beginning of the file - 1 */
    reader->mmap_offset = cp + 1 - reader->mapped_file;

    return 0;

  case BINARY_TRACE_FORMAT:
    if (reader->mmap_offset >= reader->item_size) {
      reader->mmap_offset -= (reader->item_size);
      return 0;
    } else {
      return 1;
    }
  default:
    ERROR("cannot recognize reader trace format: %d\n", reader->trace_format);
    exit(1);
  }
}

/**
 * go back two requests
 * @param reader
 * @return return 0 on successful, non-zero otherwise
 */
int go_back_two_lines(reader_t *const reader) {
//  switch (reader->trace_format) {
//  case TXT_TRACE_FORMAT:
//    if (go_back_one_line(reader) == 0) {
//      reader->item_size = 0;
//      return go_back_one_line(reader);
//    } else
//      return 1;
//  case BINARY_TRACE_FORMAT:
//    if (reader->mmap_offset >= (reader->item_size * 2)) {
//      reader->mmap_offset -= (reader->item_size) * 2;
//      return 0;
//    } else {
//      return -1;
//    }
//  default:
//    ERROR("cannot recognize reader trace format: %d\n", reader->trace_format);
//    exit(1);
//  }

  if (go_back_one_line(reader) == 0) {
    return go_back_one_line(reader);
  } else {
    return 1;
  }
}


/**
 * read one request from reader precede current position,
 * in other words, read the line above current line,
 * and currently file points to either the end of current line or
 * beginning of next line
 *
 * this method is used when reading the trace from end to beginning
 * @param reader
 * @param c
 * @return
 */
int read_one_req_above(reader_t *const reader, request_t *c) {
  if (go_back_two_lines(reader) == 0) {
    return read_one_req(reader, c);
  } else {
    c->valid = false;
    return 1;
  }
}

/**
 * skip n request for txt trace
 * @param reader
 * @param N
 * @return
 */
uint64_t skip_n_req_txt(reader_t *reader, uint64_t N) {
  char *line_end = NULL;
  uint64_t count = N;
  bool end = false;
  size_t line_len;
  for (uint64_t i = 0; i < N; i++) {
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

  return count;
}

/**
 * skip the next following N elements in the trace,
 * @param reader
 * @param N
 * @return the number of elements that are actually skipped
 */
uint64_t skip_n_req(reader_t *reader, uint64_t N) {
  uint64_t count = N;

  if (reader->trace_format == TXT_TRACE_FORMAT) {
    if (reader->trace_type == CSV_TRACE) {
      csv_skip_N_elements(reader, N);
    }
    count = skip_n_req_txt(reader, N);
  } else if (reader->trace_format == BINARY_TRACE_FORMAT) {
    if (reader->mmap_offset + N * reader->item_size <= reader->file_size) {
      reader->mmap_offset = reader->mmap_offset + N * reader->item_size;
    } else {
      count = (reader->file_size - reader->mmap_offset) / reader->item_size;
      reader->mmap_offset = reader->file_size;
      WARNING("required to skip %lu requests, but only %lu requests left\n",
              (unsigned long) N, (unsigned long) count);
    }
  } else {
    ERROR("unknown trace format %d\n", reader->trace_format);
    abort();
  }

  return count;
}

void reset_reader(reader_t *const reader) {
  /* rewind the reader back to beginning */
  reader->mmap_offset = 0;
  if (reader->trace_type == CSV_TRACE) {
    csv_reset_reader(reader);
  }
}

uint64_t get_num_of_req(reader_t *const reader) {
  if (reader->n_total_req > 0)
    return reader->n_total_req;

  uint64_t old_offset = reader->mmap_offset;
  reader->mmap_offset = 0;
  uint64_t n_req = 0;

  if (reader->trace_type == CSV_TRACE ||
          reader->trace_type == PLAIN_TXT_TRACE) {
    char *line_end = NULL;
    size_t line_len;
    while (!find_line_ending(reader, &line_end, &line_len)) {
      reader->mmap_offset = (char *) line_end - reader->mapped_file;
      n_req++;
    }

    n_req++;

    /* if the trace is csv with_header, it needs to reduce by 1  */
    if (reader->trace_type == CSV_TRACE &&
        ((csv_params_t *) (reader->reader_params))->has_header) {
      n_req--;
    }
  } else {
    ERROR("non csv/txt trace should calculate "
          "the number of requests during setup\n");
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

  if (reader->trace_type == CSV_TRACE) {
    csv_params_t *params = reader->reader_params;
//    fclose(reader->file);
    csv_free(params->csv_parser);
    g_free(params->csv_parser);
  } else if (reader->trace_type == PLAIN_TXT_TRACE) {
//    fclose(reader->file);
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
  if (reader->trace_type == CSV_TRACE) {
    csv_set_no_eof(reader);
  }
}

/**
 * jump to given position in the trace, such as 0.2, 0.5, 1.0, etc.
 * because certain functions require reader to be read sequentially,
 * this function should not be used in the middle of reading the trace
 */
void reader_set_read_pos(reader_t *const reader, double pos) {
  if (pos > 1)
    pos = 1;

  reader->mmap_offset = (long) ((double) reader->file_size * pos);
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
 *  find the closest end of line (CRLF)
 *  next_line is set to point to the first character of next line
 *  line_len is the length of current line, does not include CRLF, nor \0
 *  return true, if end of file return false else
 *
 * @param reader
 * @param next_line returns the position of first char of next line
 * @param line_len  len of current line (does not include CRLF)
 * @return true, if end of file return false else
 */
bool find_line_ending(reader_t *const reader, char **next_line,
                      size_t *const line_len) {

  size_t size = MIN(MAX_LINE_LEN, (long) reader->file_size - reader->mmap_offset);
  void *start_pos = (void *) ((char *) (reader->mapped_file) + reader->mmap_offset);
  *next_line = NULL;

  while (*next_line == NULL) {
    *next_line = (char *) memchr(start_pos, CSV_LF, size);
    if (*next_line == NULL)
      *next_line = (char *) memchr(start_pos, CSV_CR, size);

    if (*next_line == NULL) {
      /* the line is too long or end of file */
      if (size == MAX_LINE_LEN) {
        WARNING("line length exceeds %d characters\n", MAX_LINE_LEN);
        abort();
      } else {
        /*  end of trace
         *  if file ending has no CRLF,
         *  then we set next_line points to end of file,
         *  and return true;
         *  if file ending has one or more CRLF, it will not arrive here,
         *  instead it goes to the next while,
         *  then file_end points to end of file, and return true
         */
        *next_line = (char *) (reader->mapped_file) + reader->file_size;
        *line_len = size;
        reader->item_size = *line_len;
        return true;
      }
    }
  }
  // currently next_line points to LFCR
  *line_len = *next_line - ((char *) start_pos);

//#define IS_CRLF_OR_SPACE(c) \
//((c) == CSV_CR || (c) == CSV_LF || (c) == CSV_TAB || (c) == CSV_SPACE)

#define MMAP_POS(cp) ((long) ((char*) (cp) - (char *) (reader->mapped_file)))
#define BEFORE_END_OF_FILE(cp) (MMAP_POS(cp) + 1 <= (long) (reader->file_size))
#define IS_END_OF_FILE(cp)     (MMAP_POS(cp) + 1 == (long) (reader->file_size))

  while (BEFORE_END_OF_FILE(*next_line + 1) && isspace(*(*next_line + 1))) {
    (*next_line)++;
  }
  /* end of file */
  if (IS_END_OF_FILE(*next_line)) {
    reader->item_size = *line_len;
    return true;
  }

  /* not end of file, points next_line to the first character of next line */
  (*next_line)++;
  reader->item_size = *line_len;

  return false;
}

void add_sampling(struct reader *reader,
                  void *sampler,
                  trace_sampling_func func) {
  reader->sampler = sampler;
  reader->sample = func;
}


#ifdef __cplusplus
}
#endif
