/**
 * convert text traces to binary traces
 *
 */

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "alibaba.h"
#include "tencent.h"
#include "traceConv.h"

using namespace std;

const size_t BUFFER_SIZE = 200000;

struct trace_req_opns {
  uint32_t ts;
  uint64_t obj_id;
  uint32_t sz;
  uint8_t op;
  uint32_t ns;  // namespace
} __attribute__((packed));

int main(int argc, char *argv[]) {
  if (argc < 4) {
    cout << "convert txt/csv trace to standardBinIQI/standardBinIQIbh\n"
         << "standardBinIQI: ts, obj_id, sz\n"
         << "standardBinIQIbh: ts, obj_id, sz, op, ns\n"
         << "Usage: ./traceConv trace_file trace_type "
         << "(alibaba/tencentBlock/tencentPhoto) format(IQI/IQIbh) per_ns (0/1)"
         << endl;
    return 0;
  }

  string trace_file = argv[1];
  string trace_type = argv[2];
  string format = argv[3];
  bool per_ns = atoi(argv[4]);

  ifstream ifs(trace_file, ios::in);
  if (!ifs.is_open()) {
    cout << "open trace file failed" << endl;
    return 0;
  }

  string output_filepath = trace_file + ".bin." + format;
  ofstream ofs(output_filepath, ios::out | ios::binary);
  if (!ofs.is_open()) {
    cout << "open output file " + output_filepath + " failed" << endl;
    return 0;
  }

  parse_func_t *parse_func = NULL;
  if (trace_type == "alibaba") {
    parse_func = parse_alibaba;
  } else if (trace_type == "tencentBlock") {
    parse_func = parse_tencent_block;
  } else if (trace_type == "tencentPhoto") {
    parse_func = parse_tencent_photo;
  } else {
    cout << "trace type not supported" << endl;
    return 0;
  }

  enum output_trace_type output_trace_type = standardIQI;
  size_t write_size = 0;
  if (format == "IQI") {
    output_trace_type = standardIQI;
    write_size = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t);
  } else if (format == "IQIbh") {
    output_trace_type = standardIQIbh;
    write_size = sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) +
                 sizeof(uint8_t) + sizeof(uint16_t);
  } else {
    cout << "format not supported" << endl;
    return 0;
  }

  uint64_t n = 0;
  auto proc_start = chrono::system_clock::now();
  uint32_t trace_start_ts = UINT32_MAX;
  string line;
  struct trace_req req;
  struct trace_req_opns req_opns;
  unordered_map<int32_t, ofstream> ofs_map;  // ns -> ofs
  unordered_map<int32_t, vector<struct trace_req>>
      buffer_map;  // ns -> per ns buffer

  while (getline(ifs, line)) {
    if (++n % 100000000 == 0) {
      chrono::duration<double> elapsed_seconds =
          chrono::system_clock::now() - proc_start;
      cout << elapsed_seconds.count() << "s: " << n / 1000000 << "M req, "
           << buffer_map.size() << " namespaces" << endl;
    }

    parse_func(line, &req);
    if (trace_start_ts == UINT32_MAX) {
      trace_start_ts = req.ts;
    }
    req.ts -= trace_start_ts;

    ofs.write((char *)&req, write_size);

    int ns = req.ns;
    if (per_ns) {
      buffer_map[req.ns].push_back(req);
      if (buffer_map[req.ns].size() >= BUFFER_SIZE) {
        if (ofs_map.find(req.ns) == ofs_map.end()) {
          string ofs_name = output_filepath + "." + to_string(req.ns);
          ofstream f(ofs_name, ios::out | ios::binary);
          if (!f) {
            cerr << "open " + ofs_name + " failed, check ulimit" << endl;
            abort();
          } else {
            ofs_map[req.ns] = std::move(f);
          }
        }

        for (auto &req : buffer_map[req.ns]) {
          if (format == "IQI") {
            ofs_map[req.ns].write((char *)&req, write_size);
          } else if (format == "IQIbh") {
            req_opns.ts = req.ts;
            req_opns.obj_id = req.obj_id;
            req_opns.sz = req.sz;
            req_opns.op = req.op;
            if (req.ns > 65535) {
              cout << "ns " << req.ns << " too large" << endl;
              abort();
            }
            req_opns.ns = req.ns;
            ofs_map[req.ns].write((char *)&req_opns, write_size);
          }
          assert(req.ns == ns);  // ns should be the same
        }
        buffer_map[req.ns].clear();
      }
    }
  }

  for (auto &t : buffer_map) {
    if (ofs_map.find(t.first) != ofs_map.end()) {
      for (auto &req : t.second) {
        assert(req.ns == t.first);
        ofs_map[req.ns].write((char *)&req, write_size);
      }
    }
    t.second.clear();
  }

  ifs.close();
  ofs.close();
  for (auto it = ofs_map.begin(); it != ofs_map.end(); it++) {
    it->second.close();
  }

  return 0;
}
