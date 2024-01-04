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
#include "customizedReader/valpinBin.h"
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
#include "generalReader/lcs.h"
#include "generalReader/libcsv.h"
#include "generalReader/readerInternal.h"

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
                       const reader_init_param_t *const init_params) {
  static bool _info_printed = false;

  int fd;
  struct stat st;
  reader_t *const reader = (reader_t *)malloc(sizeof(reader_t));
  reader->reader_params = NULL;

  /* check whether the trace is a zstd trace file,
   * currently zstd reader only supports a few binary trace */
  reader->is_zstd_file = false;
  reader->zstd_reader_p = NULL;
#ifdef SUPPORT_ZSTD_TRACE
  size_t slen = strlen(trace_path);
  if (strncmp(trace_path + (slen - 4), ".zst", 4) == 0 ||
      strncmp(trace_path + (slen - 7), ".zst.22", 7) == 0) {
    reader->is_zstd_file = true;
    reader->zstd_reader_p = create_zstd_reader(trace_path);
    if (!_info_printed) {
      VERBOSE("opening a zstd compressed data\n");
    }
  }
#endif

  reader->trace_format = INVALID_TRACE_FORMAT;
  reader->trace_type = trace_type;
  reader->n_total_req = 0;
  reader->n_read_req = 0;
  reader->ignore_size_zero_req = true;
  reader->ignore_obj_size = false;
  reader->cloned = false;
  reader->item_size = 0;
  reader->obj_id_is_num = false;
  reader->mapped_file = NULL;
  reader->mmap_offset = 0;
  reader->sampler = NULL;
  reader->trace_start_offset = 0;
  reader->read_direction = READ_FORWARD;
  reader->n_req_left = 0;
  reader->last_req_clock_time = -1;

  if (init_params != NULL) {
    memcpy(&reader->init_params, init_params, sizeof(reader_init_param_t));
    if (init_params->binary_fmt_str != NULL)
      reader->init_params.binary_fmt_str = strdup(init_params->binary_fmt_str);

    reader->ignore_obj_size = init_params->ignore_obj_size;
    reader->ignore_size_zero_req = init_params->ignore_size_zero_req;
    reader->obj_id_is_num = init_params->obj_id_is_num;
    reader->trace_start_offset = init_params->trace_start_offset;
    reader->mmap_offset = init_params->trace_start_offset;
    reader->cap_at_n_req = init_params->cap_at_n_req;
    if (init_params->sampler != NULL)
      reader->sampler = init_params->sampler->clone(init_params->sampler);
  } else {
    memset(&reader->init_params, 0, sizeof(reader_init_param_t));
  }

  assert(trace_path != NULL);
  reader->trace_path = strdup(trace_path);

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

  if (reader->trace_type == CSV_TRACE ||
      reader->trace_type == PLAIN_TXT_TRACE) {
    reader->file = fopen(reader->trace_path, "rb");
    if (reader->file == 0) {
      ERROR("Failed to open %s: %s\n", reader->trace_path, strerror(errno));
      exit(1);
    }

    reader->line_buf_size = MAX_LINE_LEN;
    reader->line_buf = (char *)malloc(reader->line_buf_size);
  } else {
    // set up mmap region
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
  }

  switch (trace_type) {
    case CSV_TRACE:
      reader->trace_format = TXT_TRACE_FORMAT;
      csv_setup_reader(reader);
      if (!check_delimiter(reader, init_params->delimiter)) {
        ERROR("The trace does not use delimiter '%c', please check\n",
              init_params->delimiter);
      }
      break;
    case PLAIN_TXT_TRACE:
      reader->trace_format = TXT_TRACE_FORMAT;
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
    case LCS_TRACE:
      LCSReader_setup(reader);
      break;
    case VALPIN_TRACE:
      valpinReader_setup(reader);
      break;
    default:
      ERROR("cannot recognize trace type: %c\n", reader->trace_type);
      abort();
  }

  if (reader->trace_format == BINARY_TRACE_FORMAT && !reader->is_zstd_file) {
    ssize_t data_region_size = reader->file_size - reader->trace_start_offset;
    if (data_region_size % reader->item_size != 0) {
      WARN(
          "trace file size %lu - %lu is not multiple of item size %lu, mod "
          "%lu\n",
          (unsigned long)reader->file_size,
          (unsigned long)reader->trace_start_offset,
          (unsigned long)reader->item_size,
          (unsigned long)reader->file_size % reader->item_size);
    }

    reader->n_total_req = (uint64_t)data_region_size / (reader->item_size);
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
  if (reader->mmap_offset >= reader->file_size) {
    DEBUG("read_one_req: end of file, current mmap_offset %zu, file size %zu\n",
          reader->mmap_offset, reader->file_size);
    req->valid = false;
    return 1;
  }

  if (reader->cap_at_n_req > 1 && reader->n_read_req >= reader->cap_at_n_req) {
    DEBUG("read_one_req: processed %ld requests capped by the user\n",
          (long) reader->n_read_req);
    req->valid = false;
    return 1;
  }

  int status = 0;
  if (reader->n_req_left > 0) {
    reader->n_req_left -= 1;
    req->clock_time = reader->last_req_clock_time;

  } else {
    size_t offset_before_read = reader->mmap_offset;

    reader->n_read_req += 1;
    req->hv = 0;
    req->ttl = -1;
    req->valid = true;

    switch (reader->trace_type) {
      case CSV_TRACE:
        offset_before_read = ftell(reader->file);
        status = csv_read_one_req(reader, req);
        break;
      case PLAIN_TXT_TRACE:;
        offset_before_read = ftell(reader->file);
        status = txt_read_one_req(reader, req);
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
      case VALPIN_TRACE:
        status = valpin_read_one_req(reader, req);
        break;
      default:
        ERROR(
            "cannot recognize reader obj_id_type, given reader obj_id_type: "
            "%c\n",
            reader->trace_type);
        abort();
    }
  }

  if (reader->sampler != NULL) {
    /* we use this complex solution rather than simply recursive calls
       because recursive calls can lead to stack overflow */
    sampler_t *sampler = reader->sampler;
    reader->sampler = NULL;
    while (!sampler->sample(sampler, req)) {
      VVERBOSE("skip one req: time %lu, obj_id %lu, size %lu at offset %zu\n",
               req->clock_time, req->obj_id, req->obj_size, offset_before_read);
      if (reader->read_direction == READ_FORWARD) {
        status = read_one_req(reader, req);
      } else {
        status = read_one_req_above(reader, req);
      }
      if (status != 0) {
        reader->sampler = sampler;
        return status;
      }
    }
    reader->sampler = sampler;
  }

  if (reader->ignore_obj_size) {
    req->obj_size = 1;
  }

  VVERBOSE("read one req: time %lu, obj_id %lu, size %lu at offset %zu\n",
           req->clock_time, req->obj_id, req->obj_size, offset_before_read);

  return status;
}

/**
 * @brief from current line/request, go back one, the next read will
 * get the current request
 *
 * @param reader
 * @return int
 */
int go_back_one_req(reader_t *const reader) {
  switch (reader->trace_format) {
    case TXT_TRACE_FORMAT:;
      ssize_t curr_offset = ftell(reader->file);
      if (curr_offset <= reader->trace_start_offset) {
        // we are at the start of the file
        return 1;
      }

      ssize_t seek_size =
          MAX_LINE_LEN - 1 > curr_offset - reader->trace_start_offset
              ? curr_offset - reader->trace_start_offset
              : MAX_LINE_LEN - 1;
      VVERBOSE("go_back_one_req prev pos %ld, seek size %zu\n",
               ftell(reader->file), seek_size);

      fseek(reader->file, -seek_size, SEEK_CUR);
      /* do not read the current pos */
      int _read_size = fread(reader->line_buf, seek_size - 1, 1, reader->file);
      reader->line_buf[seek_size - 1] = 0;
      char *last_line_end = strrchr(reader->line_buf, '\n');
      if (last_line_end == NULL) {
        if (seek_size < MAX_LINE_LEN - 1) {
          fseek(reader->file, reader->trace_start_offset, SEEK_SET);
        }
        if (curr_offset == reader->trace_start_offset) {
          /* this happens when reverse reading reaches the start of the file */
          DEBUG(
              "go_back_one_req cannot find the request above, set offset to "
              "trace start %zu\n",
              ftell(reader->file));

          // if curr_offset is 0, we were at the start of the file before seek,
          // so we return 1; otherwise, we return 0 because we seek to the start
          return 1;
        } else {
          return 0;
        }
        return curr_offset == 0 ? 1 : 0;
      }
      int pos = last_line_end + 2 - reader->line_buf;
      fseek(reader->file, -(seek_size - pos), SEEK_CUR);

      VVERBOSE("go_back_one_req after pos %ld\n", ftell(reader->file));
      return 0;

    case BINARY_TRACE_FORMAT:
      if (reader->mmap_offset >=
          reader->trace_start_offset + reader->item_size) {
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

int go_back_two_req(reader_t *const reader) {
  /* go back two requests
   return 0 on successful, non-zero otherwise
   */
  if (go_back_one_req(reader) == 0) {
    return go_back_one_req(reader);
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
  if (reader->n_req_left > 0) {
    reader->n_req_left -= 1;
    req->clock_time = reader->last_req_clock_time;
    return 0;
  }

  if (go_back_two_req(reader) == 0) {
    return read_one_req(reader, req);
  } else {
    req->valid = false;
    return 1;
  }
}

/**
 * skip the next following N elements in the trace,
 * @param reader
 * @param N
 * @return the number of elements that are actually skipped
 */
int skip_n_req(reader_t *reader, const int N) {
  int count = N;
  char **buf = &reader->line_buf;
  size_t *buf_size_ptr = &reader->line_buf_size;

  if (reader->trace_format == TXT_TRACE_FORMAT) {
    for (int i = 0; i < N; i++) {
      if (getline(buf, buf_size_ptr, reader->file) == -1) {
        WARN("try to skip %d requests, but only %d requests left\n", N, i);
        return i;
      }
    }
  } else if (reader->trace_format == BINARY_TRACE_FORMAT) {
    if (reader->mmap_offset + N * reader->item_size <= reader->file_size) {
      reader->mmap_offset = reader->mmap_offset + N * reader->item_size;
    } else {
      count = (reader->file_size - reader->mmap_offset) / reader->item_size;
      reader->mmap_offset = reader->file_size;
      WARN("try to skip %d requests, but only %d requests left\n", N, count);
    }
  } else {
    ERROR("unknown trace format %d\n", reader->trace_format);
    abort();
  }

  VERBOSE("skip %d requests\n", count);

  return count;
}

void reset_reader(reader_t *const reader) {
  /* rewind the reader back to beginning */
  long curr_offset = 0;
  if (reader->trace_type == PLAIN_TXT_TRACE) {
    fseek(reader->file, 0, SEEK_SET);
    curr_offset = ftell(reader->file);
  } else if (reader->trace_type == CSV_TRACE) {
    csv_reset_reader(reader);
    curr_offset = ftell(reader->file);
  } else {
    reader->mmap_offset = reader->trace_start_offset;
    curr_offset = reader->mmap_offset;
  }

#ifdef SUPPORT_ZSTD_TRACE
  if (reader->is_zstd_file) {
    fseek(reader->zstd_reader_p->ifile, 0, SEEK_SET);
  }
#endif

  DEBUG("reset reader current offset %ld\n", curr_offset);
}

uint64_t get_num_of_req(reader_t *const reader) {
  if (reader->n_total_req > 0) return reader->n_total_req;

  uint64_t n_req = 0;

  if (reader->trace_format == TXT_TRACE_FORMAT || reader->is_zstd_file) {
    reader_t *reader_copy = clone_reader(reader);
    reader_copy->mmap_offset = 0;
    request_t *req = new_request();
    while (read_one_req(reader_copy, req) == 0) {
      n_req++;
    }
  } else {
    ERROR("should not reach here\n");
    abort();
  }
  reader->n_total_req = n_req;
  return n_req;
}

reader_t *clone_reader(const reader_t *const reader_in) {
  reader_t *reader = setup_reader(reader_in->trace_path, reader_in->trace_type,
                                  &reader_in->init_params);
  reader->n_total_req = reader_in->n_total_req;

  if (reader->trace_format != TXT_TRACE_FORMAT) {
    munmap(reader->mapped_file, reader->file_size);
    reader->mapped_file = reader_in->mapped_file;
  }
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

  if (reader->trace_type == PLAIN_TXT_TRACE) {
    fclose(reader->file);
    free(reader->line_buf);
  } else if (reader->trace_type == CSV_TRACE) {
    csv_params_t *csv_params = reader->reader_params;
    fclose(reader->file);
    free(reader->line_buf);
    csv_free(csv_params->csv_parser);
    free(csv_params->csv_parser);
  } else if (reader->trace_type == BIN_TRACE) {
    binary_params_t *params = reader->reader_params;
    if (params != NULL && params->fmt_str != NULL) {
      free(params->fmt_str);
    }
    if (reader->init_params.binary_fmt_str != NULL) {
      free(reader->init_params.binary_fmt_str);
    }
  }

#ifdef SUPPORT_ZSTD_TRACE
  if (reader->is_zstd_file) {
    free_zstd_reader(reader->zstd_reader_p);
  }
#endif

  if (!reader->cloned) {
    if (reader->mapped_file != NULL) {
      munmap(reader->mapped_file, reader->file_size);
    }
    if (reader->init_params.sampler != NULL) {
      reader->init_params.sampler->free(reader->init_params.sampler);
    }
  }

  if (reader->reader_params != NULL) {
    free(reader->reader_params);
  }

  if (reader->sampler != NULL) {
    free(reader->sampler);
  }

  free(reader->trace_path);
  free(reader);

  return 0;
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

  size_t offset = (double)reader->file_size * pos;
  if (reader->trace_format == TXT_TRACE_FORMAT) {
    fseek(reader->file, offset, SEEK_SET);
    if (offset != 0 && offset != reader->file_size) {
      go_back_one_req(reader);
    }
    if (offset == reader->file_size) {
      char c;
      fseek(reader->file, -1, SEEK_CUR);
      int _v = fread(&c, 1, 1, reader->file);
      while (isspace(c)) {
        fseek(reader->file, -2, SEEK_CUR);
        _v = fread(&c, 1, 1, reader->file);
      }
    }
  } else {
    reader->mmap_offset = offset;
    reader->mmap_offset -= reader->mmap_offset % reader->item_size;
  }
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
  go_back_one_req(reader);
  read_one_req(reader, req);

  reader->mmap_offset = offset;
}

bool is_str_num(const char *str) {
  for (int i = 0; i < strlen(str); i++) {
    if (!(isdigit(str[i]) || (str[i] >= 'a' && str[i] <= 'f') ||
          (str[i] >= 'A' && str[i] <= 'F')))
      return false;
  }
  return true;
}

#ifdef __cplusplus
}
#endif
