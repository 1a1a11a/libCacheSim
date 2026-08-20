#include "simulator.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

extern "C" {
#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"
#include "libCacheSim/logging.h"
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
// RandomComputeSmallQueue, BeladyCompute, ...) re-sample inside evict() instead of
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

// Everything cachesim offers that prefixsim can drive. The list is verified,
// not assumed: an entry is here because it runs a whole trace with the
// post-request residency check on. Six are absent because they genuinely do not
// work yet, not because of policy -- see README "Which algorithms work".
const AlgoEntry kAlgos[] = {
    // single-queue
    {"lru", LRU_init},
    {"fifo", FIFO_init},
    {"clock", Clock_init},
    {"sieve", Sieve_init},
    {"lfu", LFU_init},
    {"lfuda", LFUDA_init},
    {"mru", MRU_init},
    {"size", Size_init},
    {"random", Random_init},
    {"randomtwo", RandomTwo_init},
    {"randomlru", RandomLRU_init},
    {"lru-prob", LRU_Prob_init},
    {"fifo-merge", FIFO_Merge_init},
    {"hyperbolic", Hyperbolic_init},
    // multi-queue / adaptive
    {"arc", ARC_init},
    {"twoq", TwoQ_init},
    {"lirs", LIRS_init},
    {"slru", SLRU_init},
    {"lecar", LeCaR_init},
    {"cacheus", Cacheus_init},
    {"wtinylfu", WTinyLFU_init},
    {"s3fifo", S3FIFO_init},
    {"lhd", LHD_init},
    {"s3fifod", S3FIFOd_init},
    {"qdlp", QDLP_init},
    {"s3fifo_compute", S3FIFOCompute_init},
    {"car", CAR_init},
    // cost-aware
    {"gdsf", GDSF_init},
    {"gdsf_compute", GDSF_compute_init},
    {"belady", Belady_init},
    {"belady_compute", BeladyCompute_init},
    {"random_compute", RandomCompute_init},
    {"random_compute_small_queue", RandomComputeSmallQueue_init},
    // published prefix-cache policies
    {"workload_aware", WorkloadAware_init},
    {"asym_cache", AsymCache_init},
    {"asym_cache_time", AsymCacheTime_init},
    // partial-node, prefix-tree aware
    {"partial_node_random_compute", PartialNodeRandomCompute_init},
    {"partial_node_random_freq", PartialNodeRandomFreq_init},
    {"partial_node_random_compute_small_queue",
     PartialNodeRandomComputeSmallQueue_init},
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

  if (!config_.dump_holes_path.empty()) {
    holes_file_ = fopen(config_.dump_holes_path.c_str(), "w");
    if (holes_file_ == nullptr) {
      // Fatal on purpose. Carrying on would mean waiting out the whole
      // simulation only to find the dump you asked for was never written;
      // a missing output directory should stop you now, not in ten minutes.
      ERROR("cannot open %s for the hole dump (does the directory exist?)\n",
            config_.dump_holes_path.c_str());
    }
  }
}

Simulator::~Simulator() {
  // Detach before the owner frees the cache, so cache_struct_free() does not
  // touch a recorder whose victim vector has already died with this object.
  if (cache_ != nullptr && cache_->prefetcher == recorder_) {
    cache_->prefetcher = nullptr;
  }
  delete recorder_;
  if (req_buf_ != nullptr) free_request(req_buf_);
  if (holes_file_ != nullptr) fclose(holes_file_);
}

bool Simulator::probe(obj_id_t id) const {
  // The algorithm's own read-only lookup. cache_find_base gates every mutation
  // on update_cache, so this records nothing -- freq, recency and queue order
  // are all untouched, which is what phase 1 requires.
  //
  // Deliberately not a raw hashtable probe: an algorithm's resident set is not
  // always its main hash table. The S3FIFO family keeps blocks in sub-caches
  // with their own tables (a raw probe reports them missing), while ARC and
  // LIRS keep ghost entries in the main table (a raw probe reports those as
  // hits). Asking the algorithm is the only way to get residency right for
  // both.
  request_t probe_req;
  memset(&probe_req, 0, sizeof(probe_req));
  probe_req.obj_id = id;
  probe_req.obj_size = 1;
  probe_req.valid = true;
  return cache_->find(cache_, &probe_req, false) != nullptr;
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
  //
  // Warned once rather than per request, because this is a property of the
  // cache size and not of the individual request: pick a size below the largest
  // prompt in the trace and it fires for a whole class of requests, which at
  // trace scale would bury every other line of output. The exact count is
  // reported as n_req_skipped in the RESULT line, so nothing is lost by staying
  // quiet after the first -- but a silent skip is worth warning about at all,
  // because it silently narrows what the reported ratios are averaged over.
  if (static_cast<int64_t>(alpha_.size()) > config_.cache_size_blocks) {
    WARN_ONCE(
        "request %ld needs %ld distinct blocks, more than the whole cache "
        "(%ld); skipping it and excluding it from every ratio. Further "
        "oversized requests are skipped silently -- see n_req_skipped.\n",
        (long)request.index, (long)alpha_.size(),
        (long)config_.cache_size_blocks);
    ++stats_.n_requests_skipped;
    return true;
  }
  ++stats_.n_requests;

  // ------- hand the request's own properties to the algorithm -------
  // Before phase 2, because this is what an algorithm needs while it is picking
  // victims *for this request*: a time-decay policy's "now" and the workload
  // class a newly admitted block belongs to. record_request (below) runs after
  // allocation and would lag both by one request. Placement matches
  // set_request_context() in the vLLM prototype's harness.
  if (cache_->set_request_ctx != nullptr) {
    cache_request_ctx_t ctx;
    ctx.timestamp = request.timestamp;
    ctx.category = request.category;
    ctx.n_blocks = static_cast<int64_t>(n_pos);
    cache_->set_request_ctx(cache_, &ctx);
  }

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

  if (holes_file_ != nullptr) dump_holes(request);

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
  // Occupancy freed on the no-victim-reported path since the last re-probe.
  // Lets us re-probe once per deficit-worth of evictions instead of once per
  // freed block -- see the long comment on that path below.
  int64_t unobserved_freed = 0;

  // Which of Alpha's blocks are resident right now. Only consulted on the
  // no-victim-reported path, so building it costs nothing for the algorithms
  // that do report their victims.
  alpha_resident_.clear();
  for (size_t i = 0; i < request.blocks.size(); ++i) {
    if (resident_[i] != 0) alpha_resident_.insert(request.blocks[i]);
  }

  // Termination: the request needs at most `cache_size` distinct blocks (larger
  // ones were skipped), so the cache always holds at least `needed` blocks the
  // request does not want. Every eviction of a wanted block permanently removes
  // it from the resident set, so there can be at most H of them before every
  // subsequent victim is unwanted and progress advances.
  while (progress < needed) {
    const size_t before = victims_.size();
    const int64_t occupied_before = cache_->get_occupied_byte(cache_);

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
      // No victim was reported. Either nothing was evicted, or this algorithm
      // evicts without going through cache_evict_base -- the composite ones
      // (S3FIFO family, TwoQ, LIRS, WTinyLFU, ...) evict inside sub-caches
      // whose prefetcher is not ours, so the hook never fires for them.
      //
      // Occupancy tells the two apart, and when the cache did shrink we do not
      // need victim identity at all: re-probe the request and recompute the
      // deficit from ground truth. Only the self-eviction diagnostic depends on
      // knowing who went, so that is what we lose, not correctness.
      const int64_t occupied_now = cache_->get_occupied_byte(cache_);
      if (occupied_now >= occupied_before) {
        error = "eviction made no progress while serving request " +
                std::to_string(request.index) +
                " (the algorithm evicted nothing); cache is " +
                std::to_string(occupied_now) + "/" +
                std::to_string(config_.cache_size_blocks) + " blocks";
        return false;
      }

      stats_.n_evictions += occupied_before - occupied_now;
      stats_.n_unobserved_eviction_rounds += 1;

      // The re-probe below recovers the deficit and the self-eviction count from
      // ground truth, but it is O(|request|). Doing it after every single freed
      // block -- which is what these algorithms give us, one eviction per
      // evict() call -- makes allocation O(|request| * deficit) and dominates
      // runtime on large requests at small caches (measured ~300x slower than
      // LRU for S3FIFO/TwoQ/LIRS/WTinyLFU on the freeinference trace).
      //
      // Every eviction drops occupancy by one whether the victim was wanted or
      // not, so accumulate the drop and only re-probe once we have freed a whole
      // deficit's worth. If some of those were self-evictions the request is
      // still short, which the re-probe sees as a positive `remaining` and turns
      // into another batch. Re-probes are then bounded by the number of
      // self-eviction correction rounds (typically one or two), not by the
      // deficit. The self-eviction diagnostic stays exact: alpha_resident_.erase
      // fires at most once per block regardless of how often we re-probe.
      unobserved_freed += occupied_before - occupied_now;
      if (unobserved_freed < needed) continue;

      int64_t missing = 0;
      for (const obj_id_t id : alpha_) {
        if (probe(id)) continue;
        ++missing;
        if (alpha_resident_.erase(id) != 0) ++stats_.n_self_evictions;
      }
      const int64_t remaining =
          missing - (config_.cache_size_blocks - occupied_now);
      if (remaining <= 0) return true;
      needed = remaining;
      progress = 0;
      unobserved_freed = 0;
      continue;
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

/**
 * Write this request's holes: the contiguous runs of blocks that were missing
 * when it arrived and therefore have to be recomputed.
 *
 * Read straight off resident_, which phase 1 has already filled, so this costs
 * one pass over the request and never perturbs the simulation.
 *
 * The line format and the file naming are fixed by evaluate/analyze_holes.py,
 * which parses `(idx: S, len: L)` pairs and derives dataset/size/algorithm from
 * the file name. A line is written for every served request even when it has no
 * holes -- that script counts holes per request, so the zeros matter.
 * Skipped requests are omitted, as they are from every other statistic.
 */
void Simulator::dump_holes(const Request &request) {
  fprintf(holes_file_, "Request %lld:", static_cast<long long>(request.index));

  const int64_t n = static_cast<int64_t>(request.blocks.size());
  int64_t start = -1;
  for (int64_t i = 0; i < n; ++i) {
    const bool missing = resident_[static_cast<size_t>(i)] == 0;
    if (missing && start < 0) {
      start = i;
    } else if (!missing && start >= 0) {
      fprintf(holes_file_, " (idx: %lld, len: %lld)",
              static_cast<long long>(start), static_cast<long long>(i - start));
      start = -1;
    }
  }
  if (start >= 0) {
    fprintf(holes_file_, " (idx: %lld, len: %lld)",
            static_cast<long long>(start), static_cast<long long>(n - start));
  }
  fprintf(holes_file_, "\n");
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
