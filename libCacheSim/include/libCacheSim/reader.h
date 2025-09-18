/**
 * @file reader.h
 * @brief Defines the trace reader structures and functions.
 *
 * This file contains the definitions for `reader_t` and related structures
 * used to read and parse various cache trace formats, including text, CSV,
 * and different binary formats. It supports features like mmap for performance,
 * zstd decompression, and trace sampling.
 */

#ifndef READER_H
#define READER_H

#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "const.h"
#include "enum.h"
#include "logging.h"
#include "request.h"
#include "sampling.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialization parameters for a trace reader.
 *
 * This structure is used to configure the reader's behavior, specifying
 * field mappings for CSV/binary traces, and other options like sampling.
 */
typedef struct {
  bool ignore_obj_size;       /**< If true, treat all object sizes as 1. */
  bool ignore_size_zero_req;  /**< If true, ignore requests with an object size of 0. */
  bool obj_id_is_num;         /**< If true, object IDs are treated as numeric values. */
  bool obj_id_is_num_set;     /**< Internal flag to check if obj_id_is_num was user-specified. */
  int64_t cap_at_n_req;       /**< Stop reading after this many requests. -1 for no limit. */

  // Field indices (1-based) for various trace formats
  int32_t time_field;
  int32_t obj_id_field;
  int32_t obj_size_field;
  int32_t op_field;
  int32_t ttl_field;
  int32_t cnt_field;
  int32_t tenant_field;
  int32_t next_access_vtime_field;

  int32_t n_feature_fields;
  int32_t feature_fields[N_MAX_FEATURES];

  int32_t block_size;         /**< For block caches, splits large requests into multiple requests of this size. */

  // CSV specific parameters
  bool has_header;            /**< If true, the CSV file has a header line to be skipped. */
  bool has_header_set;        /**< Internal flag to check if has_header was user-specified. */
  char delimiter;             /**< The delimiter character for CSV files. */

  ssize_t trace_start_offset; /**< Start reading from this byte offset in the file. */

  // Binary specific parameters
  char *binary_fmt_str;       /**< A format string describing the binary trace structure. */

  sampler_t *sampler;         /**< A sampler to apply to the trace. */
} reader_init_param_t;

/**
 * @brief Direction for reading the trace file.
 */
enum read_direction {
  READ_FORWARD = 0,   /**< Read the trace from beginning to end. */
  READ_BACKWARD = 1,  /**< Read the trace from end to beginning. */
};

struct zstd_reader;

/**
 * @brief The main trace reader structure.
 *
 * Holds the state for reading a trace file, including file handles,
 * memory-mapped regions, and parsing state.
 */
typedef struct reader {
  /************* common fields *************/
  int64_t n_read_req;       /**< Number of requests read so far. */
  int64_t n_total_req;      /**< Total number of requests in the trace (if known). */
  char *trace_path;         /**< Path to the trace file. */
  size_t file_size;         /**< Size of the trace file in bytes. */
  reader_init_param_t init_params; /**< The initialization parameters used. */
  void *reader_params;      /**< Parameters for the specific trace format reader. */
  trace_type_e trace_type;  /**< The type of the trace. */
  trace_format_e trace_format; /**< The format of the trace (e.g., text, binary). */
  int ver;                  /**< Version number for certain trace formats. */
  bool cloned;              /**< True if this is a cloned reader instance. */
  int64_t cap_at_n_req;     /**< The maximum number of requests to read. */
  int trace_start_offset;   /**< The byte offset of the first request in the trace. */

  /************* used by binary trace *************/
  char *mapped_file;        /**< Pointer to the memory-mapped file. */
  size_t mmap_offset;       /**< Current offset in the memory-mapped file. */
  struct zstd_reader *zstd_reader_p; /**< Pointer to the zstd decompression state. */
  bool is_zstd_file;        /**< True if the trace file is zstd compressed. */
  size_t item_size;         /**< The size of a single request record in a binary trace. */

  /************* used by txt trace *************/
  FILE *file;               /**< File pointer for text-based traces. */
  char *line_buf;           /**< Buffer for reading lines from the file. */
  size_t line_buf_size;     /**< Size of the line buffer. */
  char csv_delimiter;       /**< Delimiter for CSV traces. */
  bool csv_has_header;      /**< Flag for CSV header. */

  bool obj_id_is_num;       /**< Whether object IDs are numeric. */
  bool obj_id_is_num_set;   /**< Whether obj_id_is_num was user-specified. */

  bool ignore_size_zero_req;/**< Whether to ignore zero-sized requests. */
  bool ignore_obj_size;     /**< Whether to ignore object sizes from the trace. */

  int32_t block_size;       /**< Block size for block cache traces. */

  int n_req_left;           /**< Number of sub-requests left to generate from a larger request. */
  int64_t last_req_clock_time; /**< Timestamp of the last processed request. */

  int64_t lcs_ver;          /**< Version of the LCS trace format. */

  sampler_t *sampler;       /**< Sampler being used. */
  enum read_direction read_direction; /**< The direction of reading. */
} reader_t;

/**
 * @brief Sets the default values for reader initialization parameters.
 * @param params A pointer to the `reader_init_param_t` struct to initialize.
 */
static inline void set_default_reader_init_params(reader_init_param_t *params) {
  memset(params, 0, sizeof(reader_init_param_t));

  params->ignore_obj_size = false;
  params->ignore_size_zero_req = true;
  params->obj_id_is_num = true;
  params->obj_id_is_num_set = false;
  params->cap_at_n_req = -1;
  params->trace_start_offset = 0;

  params->has_header = false;
  params->has_header_set = false;
  params->delimiter = ',';

  params->block_size = -1;
  params->binary_fmt_str = NULL;

  params->sampler = NULL;
}

/**
 * @brief Returns a `reader_init_param_t` struct with default values.
 * @return An initialized `reader_init_param_t` struct.
 */
static inline reader_init_param_t default_reader_init_params(void) {
  reader_init_param_t init_params;
  set_default_reader_init_params(&init_params);
  return init_params;
}

/**
 * @brief Sets up a reader for a given trace file.
 * @param trace_path Path to the trace file.
 * @param trace_type The type of the trace (e.g., CSV, BINARY, VSCSI).
 * @param reader_init_param Initialization parameters for the reader.
 * @return A pointer to an initialized `reader_t` struct, or NULL on failure.
 *         The returned reader must be freed with `close_reader`.
 */
reader_t *setup_reader(const char *trace_path, trace_type_e trace_type,
                       const reader_init_param_t *reader_init_param);

/**
 * @brief An alias for `setup_reader`.
 */
static inline reader_t *open_trace(
    const char *path, const trace_type_e type,
    const reader_init_param_t *reader_init_param) {
  return setup_reader(path, type, reader_init_param);
}

/**
 * @brief Gets the total number of requests in the trace.
 * @param reader The trace reader.
 * @return The total number of requests.
 */
int64_t get_num_of_req(reader_t *reader);

/**
 * @brief Gets the trace type.
 * @param reader The trace reader.
 * @return The `trace_type_e` enum value.
 */
static inline trace_type_e get_trace_type(const reader_t *const reader) {
  return reader->trace_type;
}

/**
 * @brief Checks if the object IDs in the trace are numeric.
 * @param reader The trace reader.
 * @return True if object IDs are numeric, false otherwise.
 */
static inline bool obj_id_is_num(const reader_t *const reader) {
  return reader->obj_id_is_num;
}

/**
 * @brief Reads one request from the trace.
 * @param reader The trace reader.
 * @param req A pointer to a `request_t` struct to be filled with the request data.
 * @return 0 on success, 1 if the end of the trace is reached.
 */
int read_one_req(reader_t *reader, request_t *req);

/**
 * @brief An alias for `read_one_req`.
 */
static inline int read_trace(reader_t *const reader, request_t *const req) {
  return read_one_req(reader, req);
}

/**
 * @brief Resets the reader to the beginning of the trace.
 * @param reader The trace reader to reset.
 */
void reset_reader(reader_t *reader);

/**
 * @brief Closes the reader and releases all associated resources.
 * @param reader The trace reader to close.
 * @return 0 on success.
 */
int close_reader(reader_t *reader);

/**
 * @brief An alias for `close_reader`.
 */
static inline int close_trace(reader_t *const reader) {
  return close_reader(reader);
}

/**
 * @brief Creates a new reader that is a clone of an existing one.
 *
 * This is useful for multi-threaded simulations where each thread needs its own reader.
 * @param reader The reader to clone.
 * @return A pointer to the new `reader_t` instance.
 */
reader_t *clone_reader(const reader_t *reader);

/**
 * @brief Reads the very first request of the trace.
 * @param reader The trace reader.
 * @param req A pointer to a `request_t` struct to store the result.
 */
void read_first_req(reader_t *reader, request_t *req);

/**
 * @brief Reads the very last request of the trace.
 * @param reader The trace reader.
 * @param req A pointer to a `request_t` struct to store the result.
 */
void read_last_req(reader_t *reader, request_t *req);

/**
 * @brief Skips a specified number of requests in the trace.
 * @param reader The trace reader.
 * @param N The number of requests to skip.
 * @return 0 on success.
 */
int skip_n_req(reader_t *reader, int N);

/**
 * @brief Reads requests until one with a timestamp greater than the given request is found.
 * @param reader The trace reader.
 * @param c The request to compare against.
 * @return 0 on success, 1 on end of trace.
 */
int read_one_req_above(reader_t *reader, request_t *c);

/**
 * @brief Moves the reader position back by one request.
 * @param reader The trace reader.
 * @return 0 on success.
 */
int go_back_one_req(reader_t *reader);

/**
 * @brief Sets the reader's position to a specified fraction of the trace.
 * @param reader The trace reader.
 * @param pos The position, from 0.0 (beginning) to 1.0 (end).
 */
void reader_set_read_pos(reader_t *reader, double pos);

/**
 * @brief Prints the current state of the reader for debugging.
 * @param reader The trace reader.
 */
static inline void print_reader(reader_t *reader) {
  printf(
      "trace_type: %s, trace_path: %s, trace_start_offset: %d, mmap_offset: "
      "%lu, is_zstd_file: %d, item_size: %zu, file: %p, line_buf: "
      "%s, line_buf_size: %zu, csv_delimiter: %c, csv_has_header: %d, "
      "obj_id_is_num: %d, ignore_size_zero_req: %d, ignore_obj_size: %d, "
      "n_req_left: %d, last_req_clock_time: %ld\n",
      g_trace_type_name[reader->trace_type], reader->trace_path,
      reader->trace_start_offset, (long)reader->mmap_offset,
      reader->is_zstd_file, reader->item_size, (void *)reader->file,
      reader->line_buf ? reader->line_buf : "NULL", reader->line_buf_size,
      reader->csv_delimiter, reader->csv_has_header, reader->obj_id_is_num,
      reader->ignore_size_zero_req, reader->ignore_obj_size, reader->n_req_left,
      (long)reader->last_req_clock_time);
}

#ifdef __cplusplus
}
#endif

#endif /* reader_h */
