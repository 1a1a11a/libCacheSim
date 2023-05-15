#pragma once

#include <inttypes.h>

#include <cstring>


struct trace_req {
  uint32_t ts;
  uint64_t obj_id;
  uint32_t sz;
  uint8_t op;
  uint32_t ns;  // namespace
} __attribute__((packed));


// enum op {
//     OP_INVALID = 0,
//     OP_READ = 1,
//     OP_WRITE = 3,
//     OP_UNKNOWN = 4
// };

enum output_trace_type {
  standardIQI = 1,
  standardIQIbh = 2,  // opNS
};
