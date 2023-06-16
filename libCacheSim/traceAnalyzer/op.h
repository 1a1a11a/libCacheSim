

#include <iomanip>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "../include/libCacheSim/request.h"

using namespace std;

namespace traceAnalyzer {

class OpStat {
 public:
  OpStat() = default;
  ~OpStat() = default;

  inline void add_req(request_t* req) {
    op_cnt_[req->op] += 1;
    if (req->overwrite) overwrite_cnt_ += 1;
  }

  friend ostream& operator<<(ostream& os, const OpStat& op) {
    stringstream stat_ss;
    uint64_t n_req = accumulate(op.op_cnt_, op.op_cnt_ + OP_INVALID + 1, 0UL);
    uint64_t n_write =
        op.op_cnt_[OP_SET] + op.op_cnt_[OP_REPLACE] + op.op_cnt_[OP_CAS];
    uint64_t n_del = op.op_cnt_[OP_DELETE];
    uint64_t n_overwrite = op.overwrite_cnt_;

    if (op.op_cnt_[OP_INVALID] < n_req / 2) {
      stat_ss << fixed << setprecision(4) << "op: ";
      for (int i = 0; i < OP_INVALID + 1; i++) {
        stat_ss << req_op_str[i] << ":" << op.op_cnt_[i] << "("
                << (double)op.op_cnt_[i] / (double)n_req << "), ";
      }
      stat_ss << "\n";
    }
    stat_ss << "write: " << n_write << "(" << (double)n_write / (double)n_req
            << "), "
            << "overwrite: " << n_overwrite << "("
            << (double)n_overwrite / (double)n_req << "), "
            << "del:" << n_del << "(" << (double)n_del / (double)n_req << ")\n";
    os << stat_ss.str();

    return os;
  }

 private:
  uint64_t op_cnt_[OP_INVALID + 1] = {0}; /* the number of requests of an op */
  uint64_t overwrite_cnt_ = 0;
};
}  // namespace traceAnalyzer