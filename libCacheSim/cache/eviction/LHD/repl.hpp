#pragma once

#include <string>

#include "candidate.hpp"

namespace LHDCache {
class Cache;
};

namespace repl {

const float NULL_MAX_RANK = 1;

class Policy {
 public:
  Policy() {}
  virtual ~Policy() {}

  virtual void update(candidate_t id, const request_t *req) = 0;
  virtual void replaced(candidate_t id) = 0;
  virtual candidate_t rank(const request_t *req) = 0;

  virtual void dumpStats(LHDCache::Cache *cache) {}
};

}  // namespace repl
