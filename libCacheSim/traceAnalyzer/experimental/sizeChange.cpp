
#include "sizeChange.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "../../dataStructure/robin_hood.h"
#include "../../include/libCacheSim/request.h"
#include "../utils/include/utils.h"

namespace traceAnalyzer {

using namespace std;

int SizeChangeDistribution::absolute_change_to_array_pos(int absolute_change) {
  static int mid_pos = n_bins_absolute / 2;
  if (absolute_change == 0) return mid_pos;

  int pos = mid_pos;
  if (absolute_change > 0) {
    unsigned change = absolute_change;
    while (change > 0) {
      change = change >> 1u;
      pos += 1;
    }
  } else {
    unsigned change = -absolute_change;
    while (change > 0) {
      change = change >> 1u;
      pos -= 1;
    }
  }

  if (pos >= n_bins_absolute)
    pos = n_bins_absolute - 1;
  else if (pos < 0)
    pos = 0;

  return pos;
}

void SizeChangeDistribution::add_req(request_t *req) {
  n_req_total_ += 1;
  if (req->overwrite) {
    int absolute_size_change = (int)req->obj_size - (int)req->prev_size;
    double relative_size_change =
        (double)absolute_size_change / (double)req->prev_size;
    absolute_size_change_cnt_[absolute_change_to_array_pos(
        absolute_size_change)]++;
    relative_size_change_cnt_[relative_change_to_array_pos(
        relative_size_change)]++;

    //      if (absolute_size_change > 4096) {
    //        print_request(req);
    //        printf("%lf\n", relative_size_change);
    //        printf("%ld %ld %d %ld\n", req->prev_size, req->obj_size,
    //               relative_change_to_array_pos(relative_size_change),
    //               relative_size_change_cnt_[relative_change_to_array_pos(relative_size_change)]);
    //      }
  }
  //    if (req->obj_id == 3102)
  //      print_request(req);
}

};  // namespace traceAnalyzer
