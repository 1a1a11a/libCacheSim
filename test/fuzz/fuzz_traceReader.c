/**
 * libFuzzer target for the trace readers.
 *
 * A trace is untrusted input: users download traces from public repositories
 * and feed them to the simulator, so the readers must not corrupt memory on a
 * malformed file. This target writes the fuzz input to a temporary file, opens
 * it with one of the readers, and reads every request.
 *
 * The first input byte picks the reader and its options; the remaining bytes
 * are the trace. The readers abort() on input they detect as malformed (a bad
 * magic number, an unknown version), which libFuzzer would report as a crash,
 * so the target builds the parts that are checked that way -- the lcs header
 * -- and fuzzes what the readers parse without checking: text lines, csv
 * fields and binary records.
 *
 * Build and run from the repository root, as .github/workflows/fuzz.yml does
 * (on Ubuntu, clang's sanitizer runtimes are in libclang-rt-dev):
 *   cmake -G Ninja -B _build_fuzz -DENABLE_FUZZING=ON -DENABLE_TESTS=OFF \
 *         -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
 *         -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLOG_LEVEL=error
 *   ninja -C _build_fuzz fuzz_traceReader
 *   bash test/fuzz/make_seed_corpus.sh corpus
 *   _build_fuzz/bin/fuzz_traceReader corpus -max_total_time=60
 *
 * To reproduce a crash, pass the crashing input instead of the corpus:
 *   _build_fuzz/bin/fuzz_traceReader crash-<hash>
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libCacheSim/reader.h"
#include "libCacheSim/request.h"
#include "traceReader/customizedReader/lcs.h"

/* stop after this many requests so a large input cannot stall the fuzzer */
#define MAX_REQ_PER_INPUT 100000

static char trace_path[] = "/tmp/libCacheSim_fuzz_XXXXXX";
static int trace_fd = -1;

static void remove_trace_file(void) {
  if (trace_fd >= 0) {
    close(trace_fd);
    unlink(trace_path);
  }
}

int LLVMFuzzerInitialize(int *argc, char ***argv) {
  (void)argc;
  (void)argv;
  trace_fd = mkstemp(trace_path);
  if (trace_fd < 0) {
    perror("mkstemp");
    abort();
  }
  atexit(remove_trace_file);
  return 0;
}

static void write_all(const void *buf, size_t len) {
  const uint8_t *p = (const uint8_t *)buf;
  while (len > 0) {
    ssize_t n = write(trace_fd, p, len);
    if (n <= 0) {
      perror("write");
      abort();
    }
    p += n;
    len -= (size_t)n;
  }
}

/* replace the content of the trace file with header + body */
static void write_trace(const void *header, size_t header_len,
                        const uint8_t *body, size_t body_len) {
  if (ftruncate(trace_fd, 0) != 0 || lseek(trace_fd, 0, SEEK_SET) != 0) {
    perror("reset trace file");
    abort();
  }
  if (header_len > 0) write_all(header, header_len);
  if (body_len > 0) write_all(body, body_len);
}

static void read_whole_trace(reader_t *reader) {
  request_t *req = new_request();
  int64_t n_req = 0;
  while (n_req < MAX_REQ_PER_INPUT && read_one_req(reader, req) == 0) {
    n_req++;
  }
  free_request(req);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 1) return 0;
  const uint8_t selector = data[0];
  const uint8_t *body = data + 1;
  const size_t body_len = size - 1;

  reader_init_param_t params = default_reader_init_params();
  /* bit 3: whether zero-size requests are skipped, which takes a separate
   * path in the binary readers */
  params.ignore_size_zero_req = (selector & 0x08) != 0;
  trace_type_e trace_type;

  switch (selector & 0x07) {
    case 0:
    case 1: {
      /* csv: time, obj_id, obj_size. The reader aborts when the first line
       * does not contain the delimiter, so the target writes that line -- a
       * header or a request -- and fuzzes the lines after it. */
      static const char delims[] = {',', '\t', ' ', '|'};
      const char d = delims[(selector >> 4) & 0x03];
      char first_line[32];
      params.has_header = (selector & 0x01) != 0;
      snprintf(first_line, sizeof(first_line),
               params.has_header ? "time%cid%csize\n" : "1%c2%c3\n", d, d);
      trace_type = CSV_TRACE;
      params.time_field = 1;
      params.obj_id_field = 2;
      params.obj_size_field = 3;
      params.delimiter = d;
      params.has_header_set = true;
      params.obj_id_is_num = (selector & 0x40) != 0;
      params.obj_id_is_num_set = true;
      write_trace(first_line, strlen(first_line), body, body_len);
      break;
    }
    case 2:
      /* plain text: one object id per line. A numeric id that does not parse
       * is reported with ERROR (abort), so only string ids are fuzzed. */
      trace_type = PLAIN_TXT_TRACE;
      params.obj_id_is_num = false;
      params.obj_id_is_num_set = true;
      write_trace(NULL, 0, body, body_len);
      break;
    case 3:
    case 4:
      /* oracleGeneral: fixed-size binary records, no header */
      trace_type = ORACLE_GENERAL_TRACE;
      write_trace(NULL, 0, body, body_len);
      break;
    default: {
      /* lcs: a valid header (it is checked with abort()) followed by
       * fuzzed records; the version decides the record layout */
      static lcs_trace_header_t header;
      memset(&header, 0, sizeof(header));
      header.start_magic = LCS_TRACE_START_MAGIC;
      header.end_magic = LCS_TRACE_END_MAGIC;
      header.version = 1 + ((selector >> 4) & 0x07);
      trace_type = LCS_TRACE;
      write_trace(&header, sizeof(header), body, body_len);
      break;
    }
  }

  /* oracleGeneral has no header, so an empty body is an empty file, and the
   * binary readers abort when they cannot mmap it */
  if (trace_type == ORACLE_GENERAL_TRACE && body_len == 0) return 0;

  reader_t *reader = open_trace(trace_path, trace_type, &params);
  read_whole_trace(reader);
  close_trace(reader);
  return 0;
}
