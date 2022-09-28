//
//  reader.c
//  libCacheSim
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include <ctype.h>

#include "../include/libCacheSim/macro.h"
#include "customizedReader/akamaiBin.h"
#include "customizedReader/cf1Bin.h"
#include "customizedReader/oracle/oracleAkamaiBin.h"
#include "customizedReader/oracle/oracleCF1Bin.h"
#include "customizedReader/oracle/oracleGeneralBin.h"
#include "customizedReader/oracle/oracleTwrBin.h"
#include "customizedReader/oracle/oracleTwrNSBin.h"
#include "customizedReader/oracle/oracleWikiBin.h"
#include "customizedReader/standardBin.h"
#include "customizedReader/twrBin.h"
#include "customizedReader/twrNSBin.h"
#include "customizedReader/vscsi.h"
#include "customizedReader/wikiBin.h"
#include "generalReader/binary.h"
#include "generalReader/csv.h"
#include "generalReader/libcsv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_TAB 0x09
#define FILE_SPACE 0x20
#define FILE_CR 0x0d
#define FILE_LF 0x0a
#define FILE_COMMA 0x2c
#define FILE_QUOTE 0x22

bool find_line_ending(reader_t *const reader, char **next_line,
                      size_t *const line_len);

reader_t *setup_reader(const char *const trace_path,
                       const trace_type_e trace_type,
                       const obj_id_type_e obj_id_type,
                       const reader_init_param_t *const reader_init_param) {
  static bool _info_printed = false;

  int fd;
  struct stat st;
  reader_t *const reader = (reader_t *)malloc(sizeof(reader_t));
  reader->reader_params = NULL;

  /* check whether the trace is a zstd trace file,
   * currently zstd reader only supports a few binary trace */
  reader->is_zstd_file = false;
  reader->zstd_reader_p = NULL;
  size_t slen = strlen(trace_path);
#ifdef SUPPORT_ZSTD_TRACE
  if (strncmp(trace_path + (slen - 4), ".zst", 4) == 0 ||
      strncmp(trace_path + (slen - 7), ".zst.22", 7) == 0) {
    reader->is_zstd_file = true;
    reader->zstd_reader_p = create_zstd_reader(trace_path);
    if (!_info_printed) {
      VERBOSE("opening a zstd compressed data\n");
    }
  }
#endif

  reader->obj_id_type = obj_id_type;
  reader->trace_format = INVALID_TRACE_FORMAT;
  reader->trace_type = trace_type;
  reader->n_total_req = 0;
  reader->n_read_req = 0;
  reader->mmap_offset = 0;
  reader->ignore_size_zero_req = true;
  reader->ignore_obj_size = false;
  reader->cloned = false;
  reader->item_size = 0;
  if (reader_init_param != NULL) {
    memcpy(&reader->init_params, reader_init_param,
           sizeof(reader_init_param_t));
    reader->ignore_obj_size = reader_init_param->ignore_obj_size;
    reader->ignore_size_zero_req = reader_init_param->ignore_size_zero_req;
    reader->obj_id_is_num = reader_init_param->obj_id_is_num;
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
#ifdef MADV_HUGEPAGE
  if (!_info_printed) {
    VERBOSE("use hugepage\n");
  }
  madvise(reader->mapped_file, st.st_size, MADV_HUGEPAGE | MADV_SEQUENTIAL);
#endif
  _info_printed = true;

  if ((reader->mapped_file) == MAP_FAILED) {
    close(fd);
    reader->mapped_file = NULL;
    ERROR("Unable to allocate %llu bytes of memory, %s\n",
          (unsigned long long)st.st_size, strerror(errno));
    abort();
  }

  switch (trace_type) {
    case CSV_TRACE:
      csv_setup_reader(reader);
      break;
    case PLAIN_TXT_TRACE:
      reader->trace_format = TXT_TRACE_FORMAT;
      reader->file = fopen(trace_path, "r");
      if (reader->file == NULL) {
        ERROR("open trace file %s failed: %s\n", trace_path, strerror(errno));
        exit(1);
      }
      break;
    case BIN_TRACE:
      binaryReader_setup(reader);
      break;
    case VSCSI_TRACE:
      vscsiReader_setup(reader);
      break;
    case TWR_TRACE:
      twrReader_setup(reader);
      break;
    case TWRNS_TRACE:
      twrNSReader_setup(reader);
      break;
    case CF1_TRACE:
      cf1Reader_setup(reader);
      break;
    case AKAMAI_TRACE:
      akamaiReader_setup(reader);
      break;
    case WIKI16u_TRACE:
      wiki2016uReader_setup(reader);
      break;
    case WIKI19u_TRACE:
      wiki2019uReader_setup(reader);
      break;
    case WIKI19t_TRACE:
      wiki2019tReader_setup(reader);
      break;
    case STANDARD_III_TRACE:
      standardBinIII_setup(reader);
      break;
    case STANDARD_IQI_TRACE:
      standardBinIQI_setup(reader);
      break;
    case STANDARD_IQQ_TRACE:
      standardBinIQQ_setup(reader);
      break;
    case STANDARD_IQIBH_TRACE:
      standardBinIQIBH_setup(reader);
      break;
    case ORACLE_GENERAL_TRACE:
      oracleGeneralBin_setup(reader);
      break;
    case ORACLE_GENERALOPNS_TRACE:
      oracleGeneralOpNS_setup(reader);
      break;
    case ORACLE_SIM_TWR_TRACE:
      oracleSimTwrBin_setup(reader);
      break;
    case ORACLE_SYS_TWRNS_TRACE:
      oracleSysTwrNSBin_setup(reader);
      break;
    case ORACLE_SIM_TWRNS_TRACE:
      oracleSimTwrNSBin_setup(reader);
      break;
    case ORACLE_CF1_TRACE:
      oracleCF1_setup(reader);
      break;
    case ORACLE_AKAMAI_TRACE:
      oracleAkamai_setup(reader);
      break;
    case ORACLE_WIKI16u_TRACE:
      oracleWiki2016uReader_setup(reader);
      break;
    case ORACLE_WIKI19u_TRACE:
      oracleWiki2019uReader_setup(reader);
      break;
    case ORACLE_WIKI19t_TRACE:
      oracleWiki2019tReader_setup(reader);
      break;
    default:
      ERROR("cannot recognize trace type: %c\n", reader->trace_type);
      exit(1);
  }

  if (reader->trace_format == BINARY_TRACE_FORMAT && !reader->is_zstd_file &&
      reader->item_size != 0 && reader->file_size % reader->item_size != 0) {
    WARN("trace file size %zu is not multiple of record size %zu\n",
         reader->file_size, reader->item_size);
  }

  if (reader->trace_format == INVALID_TRACE_FORMAT) {
    ERROR(
        "trace reader setup did not set "
        "trace format (BINARY_TRACE_FORMAT/TXT_TRACE_FORMAT)\n");
    abort();
  }

  if (reader->is_zstd_file) {
    // we cannot get the total number requests
    // from compressed trace without reading the tracee
    reader->n_total_req = 0;
  }

  close(fd);
  return reader;
}

/**
 * @brief read one request from trace file
 * 
 * @param reader 
 * @param req 
 * @return 0 if success, 1 if end of file
 */
int read_one_req(reader_t *const reader, request_t *const req) {
  if (reader->mmap_offset >= reader->file_size - 1) {
    req->valid = FALSE;
    return 1;
  }

  int status;
  reader->n_read_req += 1;
  req->hv = 0;
  req->valid = true;
  char *line_end = NULL;
  size_t line_len;
  switch (reader->trace_type) {
    case CSV_TRACE:
      status = csv_read_one_element(reader, req);
      break;
    case PLAIN_TXT_TRACE:;
      //       ############ new ############
      //       char obj_id_str[MAX_OBJ_ID_LEN];
      //       find_line_ending(reader, &line_end, &line_len);
      //       if (reader->obj_id_type == OBJ_ID_NUM) {
      //         req->obj_id = str_to_obj_id(
      //             (char *) (reader->mapped_file + reader->mmap_offset),
      //             line_len);
      //       } else {
      //         if (line_len >= MAX_OBJ_ID_LEN) {
      //           line_len = MAX_OBJ_ID_LEN - 1;
      //           ERROR("obj_id len %zu larger than MAX_OBJ_ID_LEN %d\n",
      //                 line_len, MAX_OBJ_ID_LEN);
      //           abort();
      //         }
      //         memcpy(obj_id_str, reader->mapped_file + reader->mmap_offset,
      //         line_len); obj_id_str[line_len] = 0; ERROR("do not support
      //         csv\n"); abort();
      // //        req->obj_id = (uint64_t) g_quark_from_string(obj_id_str);
      //       }
      //       reader->mmap_offset = (char *) line_end - reader->mapped_file;
      //       status = 0;
      //       break;
      //       ############ new finish ############
      static __thread char obj_id_str[MAX_OBJ_ID_LEN];
      find_line_ending(reader, &line_end, &line_len);
      if (reader->obj_id_type == OBJ_ID_NUM) {
        // req->obj_id = str_to_obj_id((reader->mapped_file +
        // reader->mmap_offset), line_len);
        req->obj_id = atoll(reader->mapped_file + reader->mmap_offset);
      } else {
        if (line_len >= MAX_OBJ_ID_LEN) {
          ERROR("obj_id len %zu larger than MAX_OBJ_ID_LEN %d\n", line_len,
                MAX_OBJ_ID_LEN);
          abort();
        }
        memcpy(obj_id_str, reader->mapped_file + reader->mmap_offset, line_len);
        obj_id_str[line_len] = 0;
        req->obj_id = (uint64_t)g_quark_from_string(obj_id_str);
      }
      reader->mmap_offset = (char *)line_end - reader->mapped_file;
      status = 0;
      break;
    case BIN_TRACE:
      status = binary_read_one_req(reader, req);
      break;
    case VSCSI_TRACE:
      status = vscsi_read_one_req(reader, req);
      break;
    case TWR_TRACE:
      status = twr_read_one_req(reader, req);
      break;
    case TWRNS_TRACE:
      status = twrNS_read_one_req(reader, req);
      break;
    case CF1_TRACE:
      status = cf1_read_one_req(reader, req);
      break;
    case AKAMAI_TRACE:
      status = akamai_read_one_req(reader, req);
      break;
    case WIKI16u_TRACE:
      status = wiki2016u_read_one_req(reader, req);
      break;
    case WIKI19u_TRACE:
      status = wiki2019u_read_one_req(reader, req);
      break;
    case WIKI19t_TRACE:
      status = wiki2019t_read_one_req(reader, req);
      break;
    case STANDARD_III_TRACE:
      status = standardBinIII_read_one_req(reader, req);
      break;
    case STANDARD_IQI_TRACE:
      status = standardBinIQI_read_one_req(reader, req);
      break;
    case STANDARD_IQQ_TRACE:
      status = standardBinIQQ_read_one_req(reader, req);
      break;
    case STANDARD_IQIBH_TRACE:
      status = standardBinIQIBH_read_one_req(reader, req);
      break;
    case ORACLE_GENERAL_TRACE:
      status = oracleGeneralBin_read_one_req(reader, req);
      break;
    case ORACLE_GENERALOPNS_TRACE:
      status = oracleGeneralOpNS_read_one_req(reader, req);
      break;
    case ORACLE_SIM_TWR_TRACE:
      status = oracleSimTwrBin_read_one_req(reader, req);
      break;
    case ORACLE_SYS_TWRNS_TRACE:
      status = oracleSysTwrNSBin_read_one_req(reader, req);
      break;
    case ORACLE_SIM_TWRNS_TRACE:
      status = oracleSimTwrNSBin_read_one_req(reader, req);
      break;
    case ORACLE_CF1_TRACE:
      status = oracleCF1_read_one_req(reader, req);
      break;
    case ORACLE_AKAMAI_TRACE:
      status = oracleAkamai_read_one_req(reader, req);
      break;
    case ORACLE_WIKI16u_TRACE:
      status = oracleWiki2016u_read_one_req(reader, req);
      break;
    case ORACLE_WIKI19u_TRACE:
      status = oracleWiki2019u_read_one_req(reader, req);
      break;
    case ORACLE_WIKI19t_TRACE:
      status = oracleWiki2019t_read_one_req(reader, req);
      break;
    default:
      ERROR(
          "cannot recognize reader obj_id_type, given reader obj_id_type: %c\n",
          reader->trace_type);
      exit(1);
  }

  if (reader->ignore_obj_size) {
    req->obj_size = 1;
  }

  return status;
}

int go_back_one_line(reader_t *const reader) {
  switch (reader->trace_format) {
    case TXT_TRACE_FORMAT:
      if (reader->mmap_offset == 0) return 1;

      const char *cp = reader->mapped_file + reader->mmap_offset - 1;
      while (isspace(*cp)) {
        if (--cp < reader->mapped_file) return 1;
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

int go_back_two_lines(reader_t *const reader) {
  /* go back two requests
   return 0 on successful, non-zero otherwise
   */
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
 * @return 0 on success
 */
int read_one_req_above(reader_t *const reader, request_t *req) {
  if (go_back_two_lines(reader) == 0) {
    return read_one_req(reader, req);
  } else {
    req->valid = false;
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
    reader->mmap_offset = (char *)line_end - reader->mapped_file;
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
      WARN("required to skip %lu requests, but only %lu requests left\n",
           (unsigned long)N, (unsigned long)count);
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
  if (reader->n_total_req > 0) return reader->n_total_req;

  uint64_t old_offset = reader->mmap_offset;
  reader->mmap_offset = 0;
  uint64_t n_req = 0;

  if (reader->trace_type == CSV_TRACE ||
      reader->trace_type == PLAIN_TXT_TRACE) {
    char *line_end = NULL;
    size_t line_len;
    while (!find_line_ending(reader, &line_end, &line_len)) {
      reader->mmap_offset = (char *)line_end - reader->mapped_file;
      n_req++;
    }

    n_req++;

    /* if the trace is csv with_header, it needs to reduce by 1  */
    if (reader->trace_type == CSV_TRACE &&
        ((csv_params_t *)(reader->reader_params))->has_header) {
      n_req--;
    }
  } else if (reader->is_zstd_file) {
    request_t *req = new_request();
    while (read_one_req(reader, req) == 0) {
      n_req++;
    }
  } else {
    ERROR(
        "non csv/txt/zstd trace should calculate "
        "the number of requests during setup\n");
    abort();
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
    fclose(reader->file);
    csv_free(params->csv_parser);
    free(params->csv_parser);
  } else if (reader->trace_type == PLAIN_TXT_TRACE) {
    fclose(reader->file);
  }

#ifdef SUPPORT_ZSTD_TRACE
  if (reader->is_zstd_file) {
    free_zstd_reader(reader->zstd_reader_p);
  }
#endif

  if (!reader->cloned) munmap(reader->mapped_file, reader->file_size);
  if (reader->reader_params) free(reader->reader_params);
  free(reader);
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
  /* jason (202004): this may not work for CSV
   */
  if (pos > 1) pos = 1;

  reader->mmap_offset = (long)((double)reader->file_size * pos);
  if (reader->trace_type == CSV_TRACE ||
      reader->trace_type == PLAIN_TXT_TRACE) {
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
  size_t size =
      MIN(MAX_LINE_LEN, (long)reader->file_size - reader->mmap_offset);
  void *start_pos =
      (void *)((char *)(reader->mapped_file) + reader->mmap_offset);
  *next_line = NULL;

  while (*next_line == NULL) {
    *next_line = (char *)memchr(start_pos, CSV_LF, size);
    if (*next_line == NULL)
      *next_line = (char *)memchr(start_pos, CSV_CR, size);

    if (*next_line == NULL) {
      /* the line is too long or end of file */
      if (size == MAX_LINE_LEN) {
        WARN("line length exceeds %d characters\n", MAX_LINE_LEN);
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
        *next_line = (char *)(reader->mapped_file) + reader->file_size;
        *line_len = size;
        reader->item_size = *line_len;
        return true;
      }
    }
  }
  // currently next_line points to LFCR
  *line_len = *next_line - ((char *)start_pos);

#define MMAP_POS(cp) ((long)((char *)(cp) - (char *)(reader->mapped_file)))
#define BEFORE_END_OF_FILE(cp) (MMAP_POS(cp) + 1 <= (long)(reader->file_size))
#define IS_END_OF_FILE(cp) (MMAP_POS(cp) + 1 == (long)(reader->file_size))

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

void read_first_req(reader_t *reader, request_t *req) {
  uint64_t offset = reader->mmap_offset;
  reset_reader(reader);
  read_one_req(reader, req);
  reader->mmap_offset = offset;
}

void read_last_req(reader_t *reader, request_t *req) {
  uint64_t offset = reader->mmap_offset;
  reset_reader(reader);
  reader_set_read_pos(reader, 1.0);
  go_back_one_line(reader);
  read_one_req(reader, req);

  reader->mmap_offset = offset;
}

#ifdef __cplusplus
}
#endif
