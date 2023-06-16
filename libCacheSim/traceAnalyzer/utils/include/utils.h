//
// Created by Juncheng Yang on 10/16/20.
//

#pragma once
#include <string>
#include <vector>

#include "../../../include/libCacheSim/logging.h"

using namespace std;

namespace utils {

template <typename T>
void vector_insert(vector<T> &vec, int pos, T v) {
  if (pos >= vec.size()) {
    /* resize the vector so that we can increase,
     * pos + 8 is to reduce the number of resize op */
    vec.resize(pos + 2, 0);
  }

  vec[pos] = v;
}

template <typename T>
void vector_incr(vector<T> &vec, int pos, T v) {
  if (pos >= vec.size()) {
    /* resize the vector so that we can increase,
     * pos + 8 is to reduce the number of resize op */
    vec.resize(pos + 2, 0);
  }

  vec[pos] += v;
}

// static inline void my_assert(bool b, const std::string &msg) {
//   if (!b) {
//     ERROR("%s", msg.c_str());
//     abort();
//   }
// }
}  // namespace utils
