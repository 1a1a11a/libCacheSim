/**
 * split traces by tenants
 *
 */

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.h"

using namespace std;

const size_t BUFFER_SIZE = 1000000;

typedef int(read_func_t)(ifstream &ifs, struct trace_req *req);

int read_oracleCF1_trace(ifstream &ifs, struct trace_req *req) {
  /* <IQQiiiQhhhbbb
   * ts, obj, sz, ttl, age, hostname, next_access_dist,
   * content (h), extension (h), n_level (h),
   * n_param, method, colo */

  uint64_t u64;
  uint64_t sz;
  uint32_t u32;
  uint16_t u16;
  uint8_t u8;

  if (ifs.read((char *)&(req->ts), sizeof(uint32_t)).eof()) {
    return 1;
  }
  ifs.read((char *)&(req->obj_id), sizeof(uint64_t));
  ifs.read((char *)&sz, sizeof(uint64_t));
  ifs.read((char *)&u32, sizeof(uint32_t));        // ttl
  ifs.read((char *)&u32, sizeof(uint32_t));        // age
  ifs.read((char *)&(req->ns), sizeof(uint32_t));  // hostname
  ifs.read((char *)&u64, sizeof(uint64_t));        // next_access_vtime
  ifs.read((char *)&u16, sizeof(uint16_t));        // content
  ifs.read((char *)&u16, sizeof(uint16_t));        // extension
  ifs.read((char *)&u16, sizeof(uint16_t));        // n_level
  ifs.read((char *)&u8, sizeof(uint8_t));          // n_param
  ifs.read((char *)&u8, sizeof(uint8_t));          // method
  ifs.read((char *)&u8, sizeof(uint8_t));          // colo
  req->sz = (uint32_t)sz;

  if (sz > UINT32_MAX) {
    if (sz > 0xFFFFFFFF00000000) {
      sz -= 0xFFFFFFFF00000000;
    }
    req->sz = UINT32_MAX;
  }

  return 0;
}

int read_oracleAkamai_trace(ifstream &ifs, struct trace_req *req) {
  /*
    req->real_time = *(uint32_t *) record;
    req->obj_id = *(uint64_t *) (record + 4);
    req->obj_size = *(uint32_t *) (record + 12);
    req->tenant_id = *(int16_t *) (record + 16) - 1;
    req->bucket_id = *(int16_t *) (record + 18) - 1;
    req->content_type = *(int16_t *) (record + 20) - 1;
    req->next_access_vtime = *(int64_t *) (record + 22);
  */

  uint64_t u64;
  uint32_t sz;
  uint32_t u32;
  uint16_t u16;

  if (ifs.read((char *)&(req->ts), sizeof(uint32_t)).eof()) {
    return 1;
  }
  ifs.read((char *)&(req->obj_id), sizeof(uint64_t));
  ifs.read((char *)&(req->sz), sizeof(uint32_t));
  ifs.read((char *)&(req->ns), sizeof(uint16_t));  // ns
  ifs.read((char *)&u16, sizeof(uint16_t));        // bucket
  ifs.read((char *)&u16, sizeof(uint16_t));        // content
  ifs.read((char *)&u64, sizeof(uint64_t));        // next_access_vtime

  if (sz > UINT32_MAX) {
    cout << "size too large " << sz << " > " << UINT32_MAX << endl;
    printf("read %u %lu %u %u\n", req->ts, req->obj_id, sz, req->ns);

    if (sz > 0xFFFFFFFF00000000) {
      sz -= 0xFFFFFFFF00000000;
    }

    req->sz = UINT32_MAX;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  cout << "split the trace by tenants, output is standardBinIQI" << endl;
  if (argc < 2) {
    cout << "Usage: " << argv[0] << " trace_file trace_type (oracleCF1/oracleAkamai) ofilepath" << endl;
    return 0;
  }

  string trace_file = argv[1];
  string trace_type = std::string(argv[2]);
  string ofilepath = argv[3];

  boost::algorithm::to_lower(trace_type);

  ifstream ifs(trace_file, ios::in | ios::binary);
  if (!ifs.is_open()) {
    cout << "open trace file failed" << endl;
    return 0;
  }

  read_func_t *read_func = NULL;
  if (trace_type == "oraclecf1") {
    read_func = read_oracleCF1_trace;
  } else if (trace_type == "oracleakamai") {
    read_func = read_oracleAkamai_trace;
  } else {
    cout << "trace type " << trace_type << " not supported" << endl;
    return 0;
  }

  /* ns -> ofs */
  unordered_map<uint32_t, ofstream> ofs_map;
  /* ns -> per ns buffer */
  unordered_map<uint32_t, vector<struct trace_req>> buffer_map;

  auto proc_start = chrono::system_clock::now();
  struct trace_req req;
  uint32_t trace_start_ts = UINT32_MAX;
  uint64_t n = 0;

  while (read_func(ifs, &req) == 0) {
    if (++n % 100000000 == 0) {
      chrono::duration<double> elapsed_sec =
          chrono::system_clock::now() - proc_start;
      cout << elapsed_sec.count() << "s: " << n / 1000000 << "M req, "
           << buffer_map.size() << "/" << ofs_map.size() << " namespaces"
           << endl;
    }

    if (trace_start_ts == UINT32_MAX) {
      trace_start_ts = req.ts;
    }
    req.ts -= trace_start_ts;

    // if (ofs_map.find(req.ns) == ofs_map.end()) {
    //   string output_filepath = ofilepath + ".ns" + to_string(req.ns) +
    //   ".bin"; ofstream ofs(output_filepath, ios::out | ios::binary); if
    //   (!ofs.is_open()) {
    //     cerr << "open " + output_filepath + " failed, check ulimit" << endl;
    //     abort();
    //   }
    //   ofs_map[req.ns] = std::move(ofs);
    // }
    // ofs_map[req.ns].write((char *)&(req.ts), sizeof(uint32_t));
    // ofs_map[req.ns].write((char *)&(req.obj_id), sizeof(uint64_t));
    // ofs_map[req.ns].write((char *)&(req.sz), sizeof(uint32_t));

    buffer_map[req.ns].push_back(req);
    if (buffer_map[req.ns].size() >= BUFFER_SIZE) {
      if (ofs_map.find(req.ns) == ofs_map.end()) {
        string output_filepath = ofilepath + ".ns" + to_string(req.ns) + ".bin";
        ofstream ofs(output_filepath, ios::out | ios::binary);
        if (!ofs.is_open()) {
          cerr << "open " + output_filepath + " failed, check ulimit" << endl;
          abort();
        }
        ofs_map[req.ns] = std::move(ofs);
      }

      for (auto &req : buffer_map[req.ns]) {
        // ofs_map[req.ns].write((char *)&req, 4+8+4);

        ofs_map[req.ns].write((char *)&(req.ts), sizeof(uint32_t));
        ofs_map[req.ns].write((char *)&(req.obj_id), sizeof(uint64_t));
        ofs_map[req.ns].write((char *)&(req.sz), sizeof(uint32_t));
      }
      buffer_map[req.ns].clear();
    }
  }

  for (auto &t : buffer_map) {
    if (ofs_map.find(t.first) != ofs_map.end()) {
      for (auto &req : t.second) {
        assert(req.ns == t.first);
        ofs_map[req.ns].write((char *)&(req.ts), sizeof(uint32_t));
        ofs_map[req.ns].write((char *)&(req.obj_id), sizeof(uint64_t));
        ofs_map[req.ns].write((char *)&(req.sz), sizeof(uint32_t));
      }
    }
    t.second.clear();
  }

  ifs.close();
  for (auto &kv : ofs_map) {
    kv.second.close();
  }

  return 0;
}
