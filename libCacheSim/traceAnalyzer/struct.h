#pragma once

#include <unordered_map>

#include "../dataStructure/robin_hood.h"
#include "../include/config.h"


typedef uint64_t obj_size_t;
// typedef int32_t time_t;

namespace traceAnalyzer {
struct obj_info {
  int64_t last_access_vtime;
  obj_size_t obj_size;
  int32_t last_access_rtime;
  uint32_t freq;
  int32_t create_rtime;
} __attribute__((packed));

using obj_info_map_type =
    robin_hood::unordered_flat_map<obj_id_t, struct obj_info>;

}  // namespace traceAnalyzer
