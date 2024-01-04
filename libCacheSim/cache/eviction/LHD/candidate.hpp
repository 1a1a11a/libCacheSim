#pragma once

#include <iostream>
#include <unordered_map>

#include "../../../include/libCacheSim/cacheObj.h"
#include "../../../include/libCacheSim/request.h"
#include "constants.hpp"

namespace repl {

struct candidate_t {
  int appId;
  int64_t id;

#if defined(TRACK_EVICTION_V_AGE)
  int64_t create_time;
#endif

  static candidate_t make(const request_t* req) {
    candidate_t c;
    c.appId = DEFAULT_APP_ID;
    c.id = static_cast<int64_t>(req->obj_id);
    return c;
  }

  inline bool operator==(const candidate_t& that) const {
    return (id == that.id) && (appId == that.appId);
  }

  inline bool operator!=(const candidate_t& that) const {
    return !(operator==(that));
  }

  inline bool operator<(const candidate_t& that) const {
    if (this->appId == that.appId) {
      return this->id < that.id;
    } else {
      return this->appId < that.appId;
    }
  }
};

const candidate_t INVALID_CANDIDATE{-1, -1};

template <typename T>
class CandidateMap : public std::unordered_map<candidate_t, T> {
 public:
  typedef std::unordered_map<candidate_t, T> Base;
  const T DEFAULT;

  CandidateMap(const T& _DEFAULT) : Base(), DEFAULT(_DEFAULT) {}

  using typename Base::const_reference;
  using typename Base::reference;

  T& operator[](candidate_t c) {
    auto itr = Base::find(c);
    if (itr == Base::end()) {
      auto ret = Base::insert({c, DEFAULT});
      assert(ret.second);
      return ret.first->second;
    } else {
      return itr->second;
    }
  }

  const T& operator[](candidate_t c) const {
    auto itr = Base::find(c);
    if (itr == Base::end()) {
      return DEFAULT;
    } else {
      return itr->second;
    }
  }
};

}  // namespace repl

// candidate_t specializations
namespace std {

template <>
struct hash<repl::candidate_t> {
  size_t operator()(const repl::candidate_t& x) const {
    return x.id;
    // return ((hash<int>()(x.appId)
    //                      ^ (hash<int>()(x.id) << 1)) >> 1);
  }
};

}  // namespace std

namespace {

inline std::ostream& operator<<(std::ostream& os, const repl::candidate_t& x) {
  return os << "(" << x.appId << ", " << x.id << ")";
}

}  // namespace
