#include "simulator.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

extern "C" {
#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"
#include "libCacheSim/macro.h"
}

namespace prefixsim {
namespace {

// ---------------------------------------------------------------------------
// Eviction recorder
//
// We need to know *which* object each eviction removed, to tell "freed a block
// the arriving request does not want" (real progress) from "freed a block the
// arriving request is about to reuse" (no progress).
//
// cache_evict_base() already invokes prefetcher->handle_evict on every genuine
// eviction, so we install a do-nothing prefetcher whose only job is to append
// the victim's id to a vector. This is the only hook in the core that reports
// the actual victim: peeking with cache->to_evict() before cache->evict() does
// NOT work, because the sampling algorithms (RandomCompute,
// RandomQuickDemotion, BeladyCompute, ...) re-sample inside evict() instead of
// reusing to_evict_candidate, so the peek names a different object.
// ---------------------------------------------------------------------------

void recorder_handle_evict(cache_t *cache, const request_t *req) {
  auto *victims = static_cast<std::vector<obj_id_t> *>(cache->prefetcher->params);
  victims->push_back(req->obj_id);
}

// cache_struct_free() calls prefetcher->free() guarded only on the prefetcher
// pointer, not on the function pointer, so this must exist even though the
// Simulator detaches the recorder before the cache is freed.
void recorder_free(prefetcher_t *prefetcher) { delete prefetcher; }

prefetcher_t *recorder_clone(prefetcher_t *prefetcher, uint64_t /*new_size*/) {
  return prefetcher;
}

/// Hash table sized to the cache, not to a default 1 GiB byte cache. The
/// sampling algorithms walk random buckets, so an oversized table makes every
/// eviction slow.
uint16_t hashpower_for(int64_t cache_size_blocks) {
  uint16_t power = 12;
  while (power < 26 && (int64_t{1} << power) < cache_size_blocks * 2) ++power;
  return power;
}

struct AlgoEntry {
  const char *name;
  cache_t *(*init)(const common_cache_params_t, const char *);
};

// Only algorithms whose entire resident set lives in cache->hashtable belong
// here; see README.md "Which algorithms work".
const AlgoEntry kAlgos[] = {
    {"lru", LRU_init},
    {"fifo", FIFO_init},
    {"clock", Clock_init},
    {"sieve", Sieve_init},
    {"lfu", LFU_init},
    {"lfuda", LFUDA_init},
    {"mru", MRU_init},
    {"size", Size_init},
    {"random", Random_init},
    {"hyperbolic", Hyperbolic_init},
    {"gdsf", GDSF_init},
    {"gdsf_compute", GDSF_compute_init},
    {"belady", Belady_init},
    {"belady_compute", BeladyCompute_init},
    {"random_compute", RandomCompute_init},
    {"random_quick_demotion", RandomQuickDemotion_init},
    {"partial_node_random_compute", PartialNodeRandomCompute_init},
    {"partial_node_random_freq", PartialNodeRandomFreq_init},
};

}  // namespace

// ---------------------------------------------------------------------------

Simulator::Simulator(cache_t *cache, const SimulatorConfig &config)
    : cache_(cache), config_(config) {
  req_buf_ = new_request();

  recorder_ = new prefetcher_t();
  recorder_->handle_evict = recorder_handle_evict;
  recorder_->free = recorder_free;
  recorder_->clone = recorder_clone;
  recorder_->params = &victims_;
  snprintf(recorder_->prefetcher_name, sizeof(recorder_->prefetcher_name),
           "prefixsim-evict-recorder");
  cache_->prefetcher = recorder_;
}

Simulator::~Simulator() {
  // Detach before the owner frees the cache, so cache_struct_free() does not
  // touch a recorder whose victim vector has already died with this object.
  if (cache_ != nullptr && cache_->prefetcher == recorder_) {
    cache_->prefetcher = nullptr;
  }
  delete recorder_;
  if (req_buf_ != nullptr) free_request(req_buf_);
}

bool Simulator::probe(obj_id_t id) const {
  // Read-only lookup straight into the hash table. Deliberately not
  // cache_->find(): even with update_cache = false that is the algorithm's own
  // code path, and phase 1 must be invisible to the algorithm.
  return hashtable_find_obj_id(cache_->hashtable, id) != nullptr;
}

void Simulator::fill_request(const Request &request, size_t i) {
  req_buf_->obj_id = request.blocks[i];
  req_buf_->obj_size = 1;
  req_buf_->cost = static_cast<int32_t>(
      llround(block_cost(config_.cost_model, static_cast<int64_t>(i))));
  req_buf_->next_access_vtime = request.next_access_vtime[i];
  req_buf_->clock_time = static_cast<int64_t>(request.timestamp);
  req_buf_->valid = true;
}

bool Simulator::run(const std::vector<Request> &requests, std::string &error) {
  for (const Request &request : requests) {
    if (!serve(request, error)) return false;
  }
  return true;
}

bool Simulator::serve(const Request &request, std::string &error) {
  const size_t n_pos = request.blocks.size();

  alpha_.clear();
  for (const obj_id_t id : request.blocks) alpha_.insert(id);

  // A request with more distinct blocks than the whole cache can never be
  // fully resident. Serving it would thrash the cache for nothing, so skip it
  // and keep it out of the ratios.
  if (static_cast<int64_t>(alpha_.size()) > config_.cache_size_blocks) {
    ++stats_.n_requests_skipped;
    return true;
  }
  ++stats_.n_requests;

  // ---------------- phase 1: match ----------------
  // Every statistic this simulator reports is computed here, against the cache
  // exactly as the request found it. Nothing in phases 2 and 3 can change it.
  resident_.assign(n_pos, 0);
  std::unordered_set<obj_id_t> missing;

  for (size_t i = 0; i < n_pos; ++i) {
    const double cost = block_cost(config_.cost_model, static_cast<int64_t>(i));
    const bool hit = probe(request.blocks[i]);
    resident_[i] = hit ? 1 : 0;

    ++stats_.n_blocks;
    stats_.total_cost += cost;
    if (hit) {
      ++stats_.n_block_hits;
      stats_.saved_cost += cost;
    } else {
      missing.insert(request.blocks[i]);
    }
  }

  // ---------------- phase 2: allocate ----------------
  const int64_t occupied = cache_->get_occupied_byte(cache_);
  const int64_t free_slots = config_.cache_size_blocks - occupied;
  const int64_t needed = static_cast<int64_t>(missing.size()) - free_slots;
  if (needed > 0 && !allocate(request, needed, error)) return false;

  // ------- hand the request's block path to the algorithm -------
  // Between making room and replaying: the algorithm must know the path before
  // it can be asked to evict against it, but the blocks are not in the cache
  // yet, so the tree records them as not-yet-resident and phase 3's inserts
  // promote them. Algorithms that do not need request structure leave this
  // hook NULL.
  if (cache_->record_request != nullptr) {
    cache_->record_request(cache_, request.blocks.data(),
                           static_cast<int64_t>(n_pos));
  }

  // ---------------- phase 3: access ----------------
  access(request);

  if (config_.verify) {
    for (size_t i = 0; i < n_pos; ++i) {
      if (!probe(request.blocks[i])) {
        error =
            "after replaying request " + std::to_string(request.index) +
            ", block " + std::to_string(i) + " of " + std::to_string(n_pos) +
            " is not resident. The eviction algorithm's resident set is not "
            "fully visible in the main hash table, so the phase-1 match cannot "
            "be trusted for it. Use one of the supported algorithms, or pass "
            "--no-verify if you know what you are doing.";
        return false;
      }
    }
  }
  return true;
}

bool Simulator::allocate(const Request &request, int64_t needed, std::string &error) {
  // Point the eviction request at the first missing block. Most algorithms
  // ignore the request argument in evict(); the cost-aware ones read req->cost.
  for (size_t i = 0; i < request.blocks.size(); ++i) {
    if (resident_[i] == 0) {
      fill_request(request, i);
      break;
    }
  }

  victims_.clear();
  int64_t progress = 0;

  // Termination: the request needs at most `cache_size` distinct blocks (larger
  // ones were skipped), so the cache always holds at least `needed` blocks the
  // request does not want. Every eviction of a wanted block permanently removes
  // it from the resident set, so there can be at most H of them before every
  // subsequent victim is unwanted and progress advances.
  while (progress < needed) {
    const size_t before = victims_.size();

    // Ask for the whole remaining deficit at once when the algorithm can do it.
    // `needed - progress` is a hard cap: this request has room for exactly that
    // many more blocks. Self-evictions do not count towards progress, so the
    // loop may still go round again -- that is correct, not a shortfall.
    if (cache_->evict_n != nullptr) {
      cache_->evict_n(cache_, req_buf_, needed - progress);
    } else {
      cache_->evict(cache_, req_buf_);
    }

    if (victims_.size() == before) {
      error = "eviction made no progress while serving request " +
              std::to_string(request.index) +
              " (the algorithm evicted nothing); cache is " +
              std::to_string(cache_->get_occupied_byte(cache_)) + "/" +
              std::to_string(config_.cache_size_blocks) + " blocks";
      return false;
    }

    for (size_t k = before; k < victims_.size(); ++k) {
      ++stats_.n_evictions;
      if (alpha_.count(victims_[k]) != 0) {
        // Freed a block this very request is about to insert again: the slot
        // is reclaimed but the demand grew by one, so it is not progress.
        ++stats_.n_self_evictions;
      } else {
        ++progress;
      }
    }
  }
  return true;
}

void Simulator::access(const Request &request) {
  // Reverse order: the deepest (most private) block is touched first and the
  // prefix root last, so recency-ordered algorithms leave the root at the MRU
  // end and evict from the deep end first. See README.md "Reverse-order replay".
  victims_.clear();
  for (int64_t i = static_cast<int64_t>(request.blocks.size()) - 1; i >= 0; --i) {
    fill_request(request, static_cast<size_t>(i));
    cache_->get(cache_, req_buf_);
  }
  // Phase 2 reserved exactly enough room, so nothing should have been evicted
  // here. If something was, the accounting is wrong -- surface it.
  stats_.n_unexpected_evictions += static_cast<int64_t>(victims_.size());
}

// ---------------------------------------------------------------------------

cache_t *create_cache_by_name(const std::string &algorithm, int64_t cache_size_blocks,
                              const char *cache_specific_params) {
  common_cache_params_t params = default_common_cache_params();
  params.cache_size = cache_size_blocks;
  params.consider_obj_metadata = false;
  params.hashpower = hashpower_for(cache_size_blocks);

  for (const AlgoEntry &entry : kAlgos) {
    if (strcasecmp(algorithm.c_str(), entry.name) == 0) {
      return entry.init(params, cache_specific_params);
    }
  }
  return nullptr;
}

const std::vector<std::string> &supported_algorithms() {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> out;
    for (const AlgoEntry &entry : kAlgos) out.emplace_back(entry.name);
    return out;
  }();
  return names;
}

}  // namespace prefixsim
