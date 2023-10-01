//
// Created by Juncheng Yang on 4/21/20.
//

#ifndef CACHESIMULATORCPP_UTILS_H
#define CACHESIMULATORCPP_UTILS_H

#include <fstream>
#include <iostream>
#include <queue>
#include <vector>

#define KB 1024
#define MB 1024 * 1024
#define GB 1024 * 1024 * 1024
#define TB 1024 * 1024 * 1024 * 1024

using namespace std;

typedef struct {
  uint32_t ts;
  uint32_t obj_id;
  uint32_t sz;
  uint32_t trace_id;
} req_t;

typedef struct comReq {
  bool operator()(req_t const& r1, req_t const& r2) { return r1.ts < r2.ts; }
} cmpReq_t;

class TraceUtils {
 public:
  static uint64_t merge_l1_trace(std::vector<std::string>& l1_trace_path,
                                 std::string& l2_output_trace_path);
};

class Utils {
 public:
  static uint64_t convert_size_str(std::string sz_str);
};

#endif  // CACHESIMULATORCPP_UTILS_H
