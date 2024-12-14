
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <iostream>
#include <unordered_map>

#include "internal.hpp"

#include "../../include/libCacheSim/logging.h"
#include "../../include/libCacheSim/reader.h"

namespace traceConv {
typedef struct oracleGeneral_req {
  uint32_t clock_time;
  uint64_t obj_id;
  uint32_t obj_size;
  int64_t next_access_vtime;

  void init(request_t *req) {
    this->clock_time = req->clock_time;
    this->obj_id = req->obj_id;
    this->obj_size = req->obj_size;
  }
} __attribute__((packed)) oracleGeneral_req_t;

struct trace_stat {
  int64_t n_req;
  int64_t n_obj;
  int64_t n_req_byte;
  int64_t n_obj_byte;
};

static void _reverse_file(std::string ofilepath, struct trace_stat stat, bool output_txt, bool remove_size_change);

/**
 * @brief Convert a trace to oracleGeneral format, which is a binary format
 *       that has time, obj_id, obj_size, next_access_vtime, where
 *       next_access_vtime is the reference count of the next access to the same
 *       object (reference count starts with 1).
 *
 * @param reader
 * @param ofilepath
 * @param sample_ratio
 * @param output_txt
 * @param remove_size_change
 */
void convert_to_oracleGeneral(reader_t *reader, std::string ofilepath, bool output_txt, bool remove_size_change) {
  request_t *req = new_request();
  std::ofstream ofile_temp(ofilepath + ".reverse", std::ios::out | std::ios::binary | std::ios::trunc);
  std::unordered_map<uint64_t, int64_t> last_access_map;

  int64_t n_req_curr = 0, n_req_total = get_num_of_req(reader);
  last_access_map.reserve(n_req_total / 100 + 1e4);

  int64_t unique_bytes = 0, total_bytes = 0, n_obj = 0;
  INFO("%s: %.2f M requests in total\n", reader->trace_path, (double)n_req_total / 1.0e6);

  reader->read_direction = READ_BACKWARD;
  reader_set_read_pos(reader, 1.0);
  go_back_one_req(reader);
  read_one_req(reader, req);

  int64_t start_ts = req->clock_time;

  oracleGeneral_req_t og_req;
  og_req.init(req);

  while (true) {
    auto last_access_it = last_access_map.find(og_req.obj_id);

    if (last_access_it == last_access_map.end()) {
      og_req.next_access_vtime = -1;
      n_obj++;
      unique_bytes += req->obj_size;
    } else {
      og_req.next_access_vtime = last_access_it->second;
    }

    total_bytes += req->obj_size;
    last_access_map[req->obj_id] = n_req_curr;

    ofile_temp.write(reinterpret_cast<char *>(&og_req), sizeof(oracleGeneral_req_t));
    n_req_curr += 1;

    if (n_req_curr % 100000000 == 0) {
      INFO(
          "%s: %ld M requests (%.2lf GB), trace time %ld, working set %lld "
          "object, %lld B (%.2lf GB)\n",
          reader->trace_path, (long)(n_req_curr / 1e6), (double)total_bytes / GiB, (long)(start_ts - req->clock_time),
          (long long)n_obj, (long long)unique_bytes, (double)unique_bytes / GiB);
    }

    if (n_req_curr > n_req_total * 2) {
      ERROR("n_req_curr (%ld) > n_req_total (%ld)\n", n_req_curr, n_req_total);
    }

    if (read_one_req_above(reader, req) != 0) {
      break;
    }
    og_req.init(req);
  }

  if (reader->sampler == nullptr) {
    assert(n_req_curr == get_num_of_req(reader));
  }

  free_request(req);
  ofile_temp.close();

  INFO(
      "%s: %ld M requests (%.2lf GB), trace time %ld, working set %lld "
      "object, %lld B (%.2lf GB), reversing output...\n",
      reader->trace_path, (long)(n_req_curr / 1e6), (double)total_bytes / GiB, (long)(start_ts - req->clock_time),
      (long long)n_obj, (long long)unique_bytes, (double)unique_bytes / GiB);

  struct trace_stat stat;
  stat.n_req = n_req_curr;
  stat.n_obj = n_obj;
  stat.n_req_byte = total_bytes;
  stat.n_obj_byte = unique_bytes;

  _reverse_file(ofilepath, stat, output_txt, remove_size_change);
}

static void _reverse_file(std::string ofilepath, struct trace_stat stat, bool output_txt, bool remove_size_change) {
  int64_t n_req = 0;
  size_t file_size;
  char *mapped_file = reinterpret_cast<char *>(utils::setup_mmap(ofilepath + ".reverse", &file_size));
  size_t pos = file_size;

  std::ofstream ofile(ofilepath, std::ios::out | std::ios::binary | std::ios::trunc);
  std::ofstream ofile_txt;
  if (output_txt) ofile_txt.open(ofilepath + ".txt", std::ios::out | std::ios::trunc);

  /* we remove object size change because some of the systems do not allow
   * object size change */
  std::unordered_map<uint64_t, uint32_t> last_obj_size;
  last_obj_size.reserve(stat.n_obj);

  oracleGeneral_req_t og_req;
  size_t req_entry_size = sizeof(oracleGeneral_req_t);

  while (pos >= req_entry_size) {
    pos -= req_entry_size;
    memcpy(&og_req, mapped_file + pos, req_entry_size);
    if (og_req.next_access_vtime != -1) {
      /* re.next_access_vtime is the vtime start from the end */
      og_req.next_access_vtime = stat.n_req - og_req.next_access_vtime;
    }

    if (remove_size_change) {
      auto it = last_obj_size.find(og_req.obj_id);
      if (it != last_obj_size.end()) {
        og_req.obj_size = it->second;
      } else {
        last_obj_size[og_req.obj_id] = og_req.obj_size;
      }
    }

    ofile.write(reinterpret_cast<char *>(&og_req), req_entry_size);
    if (output_txt) {
      ofile_txt << og_req.clock_time << "," << og_req.obj_id << "," << og_req.obj_size << ","
                << og_req.next_access_vtime << "\n";
    }

    if ((++n_req) % 100000000 == 0) {
      INFO("%s: %ld M requests\n", ofilepath.c_str(), (long)(n_req / 1e6));
    }
  }

  munmap(mapped_file, file_size);
  ofile.close();
  if (output_txt) ofile_txt.close();

  assert(n_req == stat.n_req);

  remove((ofilepath + ".reverse").c_str());

  INFO("trace conversion finished, %ld requests %ld objects, output %s\n", (long)n_req, (long)stat.n_obj,
       ofilepath.c_str());
}
}  // namespace traceConv