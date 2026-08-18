#include "partialNodeCache.hpp"

#include <algorithm>
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/macro.h"
#include "utils/include/mymath.h"
}

namespace eviction {
namespace {

inline PartialNodeCache *impl_of(const cache_t *cache) {
  return static_cast<PartialNodeCache *>(cache->eviction_params);
}

/// The block this node would give up next, or nullptr if it has none left.
cache_obj_t *node_candidate(cache_t *cache, PartialNodeCache *impl,
                            const PrefixRadixTree::Node *node) {
  const int64_t pos = impl->evict_from_tail ? impl->tree.tail_resident(node)
                                            : impl->tree.head_resident(node);
  if (pos < 0) return nullptr;
  return hashtable_find_obj_id(cache->hashtable,
                               node->blocks[static_cast<size_t>(pos)]);
}

/// Defined below, next to the eviction path it shares its scoring with.
cache_obj_t *pick_victim(cache_t *cache);

// ---------------------------------------------------------------------------
// cache_t vtable
// ---------------------------------------------------------------------------

void pn_free(cache_t *cache) {
  delete impl_of(cache);
  cache->eviction_params = nullptr;
  cache_struct_free(cache);
}

bool pn_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_obj_t *pn_find(cache_t *cache, const request_t *req, bool update_cache) {
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);
  if (obj != nullptr && update_cache) {
    obj->Random.last_access_vtime = cache->n_req;
  }
  return obj;
}

cache_obj_t *pn_insert(cache_t *cache, const request_t *req) {
  cache_obj_t *obj = cache_insert_base(cache, req);
  obj->Random.last_access_vtime = cache->n_req;
  impl_of(cache)->tree.mark_resident(obj->obj_id);
  return obj;
}

cache_obj_t *pn_to_evict(cache_t *cache, const request_t * /*req*/) {
  cache_obj_t *victim = pick_victim(cache);
  cache->to_evict_candidate = victim;
  cache->to_evict_candidate_gen_vtime = cache->n_req;
  return victim;
}

/// Drain orphans, which sampling can never reach. Returns how many went.
int64_t evict_orphans(cache_t *cache, PartialNodeCache *impl, int64_t n) {
  int64_t evicted = 0;
  while (evicted < n && impl->tree.has_orphans()) {
    const obj_id_t id = impl->tree.any_orphan();
    cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, id);
    impl->tree.mark_evicted(id);
    if (obj == nullptr) continue;  // tree drifted; the id is gone either way
    cache_evict_base(cache, obj, true);
    ++evicted;
  }
  return evicted;
}

/**
 * Score a node by one representative block -- vLLM's "canonical block".
 *
 * Scoring a whole node by one block is what makes node sampling affordable, and
 * the representative has to sit inside the chunk about to be evicted, or the
 * score describes blocks that are not going anywhere. The rule is taken from the
 * vLLM prototype verbatim so the two implementations rank nodes identically:
 *
 *   drain: start at rank min(chunk, node_size) / 2 and scan outwards over the
 *          whole node   (_get_canonical_block_for_radix_tree_node)
 *   micro: start at rank first_resident + chunk/2, clamped to the last rank,
 *          and scan outwards but never before first_resident
 *          (_get_chunk_canonical_block)
 *
 * Note both walk *ranks*, resident or not, so a sparsely-resident node can be
 * represented by a block some way from the chunk's centre. That is vLLM's
 * behaviour, not an accident of this port.
 *
 * Returns false when the node has nothing to give.
 */
bool node_chunk_score(cache_t *cache, PartialNodeCache *impl,
                      const PrefixRadixTree::Node *node, int64_t chunk,
                      double &score_out) {
  const int64_t total = static_cast<int64_t>(node->blocks.size());
  if (total == 0 || node->n_resident == 0) return false;

  obj_id_t rep = 0;
  bool found = false;
  if (impl->mode == PartialNodeCache::Mode::kMicro) {
    const int64_t first =
        impl->tree.first_resident_rank(node, impl->evict_from_tail);
    if (first < 0) return false;
    const int64_t target = std::min(first + chunk / 2, total - 1);
    found = impl->tree.canonical_resident(node, impl->evict_from_tail, target,
                                          first, rep);
  } else {
    const int64_t start = std::min(chunk, total) / 2;
    found = impl->tree.canonical_resident(node, impl->evict_from_tail, start,
                                          0, rep);
  }
  if (!found) return false;

  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, rep);
  if (obj == nullptr) return false;
  score_out = impl->score(cache, obj);
  return true;
}

/// Take up to `max_take` blocks from `node`, in eviction order.
int64_t take_from_node(cache_t *cache, PartialNodeCache *impl,
                       const PrefixRadixTree::Node *node, int64_t max_take,
                       std::vector<obj_id_t> &scratch) {
  // Snapshot the ids first: evicting the node's last resident block prunes the
  // node, and anything read from it afterwards is a use-after-free.
  scratch.clear();
  impl->tree.collect_resident(node, impl->evict_from_tail, max_take, scratch);

  int64_t took = 0;
  for (const obj_id_t id : scratch) {
    cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, id);
    if (obj == nullptr) continue;
    impl->tree.mark_evicted(id);
    cache_evict_base(cache, obj, true);
    ++took;
  }
  return took;
}

/**
 * Evict up to n objects. `n` is a hard cap.
 *
 * Per round: sample n_sample nodes, score each by its chunk midpoint, and take
 * the chunk from the best-scoring one. kDrain lets that chunk be the whole
 * remaining deficit; kMicro caps it at micro_batch and re-samples. Either way
 * the sampling cost is paid once per round rather than once per evicted block.
 *
 * Counterpart of _sample_and_evict_drain / _sample_and_evict_micro in the vLLM
 * prototype.
 */
int64_t pn_evict_n(cache_t *cache, const request_t * /*req*/, int64_t n) {
  if (n <= 0) return 0;
  PartialNodeCache *impl = impl_of(cache);
  int64_t evicted = evict_orphans(cache, impl, n);

  std::vector<obj_id_t> scratch;
  while (evicted < n) {
    const int64_t want = n - evicted;
    const int64_t chunk = (impl->mode == PartialNodeCache::Mode::kMicro)
                              ? std::min(want, impl->micro_batch)
                              : want;

    // The candidate pool is often smaller than n_sample -- a path-compressed
    // prefix tree has few, long nodes, and on the qwen traces the pool averages
    // well under a hundred. Random draws would then be strictly worse than
    // looking at everything: sampling with replacement from a pool of 78 with
    // 128 draws still misses about a fifth of it, while costing more score
    // evaluations than a full scan. So scan exhaustively once the pool fits,
    // and sample only when it genuinely does not.
    const int64_t pool = impl->tree.n_sampleable();
    if (pool == 0) break;
    const bool exhaustive = pool <= impl->n_sample;
    const int64_t draws = exhaustive ? pool : impl->n_sample;

    const PrefixRadixTree::Node *victim = nullptr;
    double best = DBL_MAX;
    for (int64_t i = 0; i < draws; ++i) {
      const PrefixRadixTree::Node *node =
          exhaustive ? impl->tree.node_at(i) : impl->tree.sample_node(next_rand());
      if (node == nullptr) break;
      double score = 0.0;
      if (!node_chunk_score(cache, impl, node, chunk, score)) continue;
      if (score < best) {
        best = score;
        victim = node;
      }
    }
    if (victim == nullptr) break;

    const int64_t took = take_from_node(cache, impl, victim, chunk, scratch);
    if (took == 0) break;  // a whole round achieved nothing; stop rather than spin
    evicted += took;
  }

  cache->to_evict_candidate_gen_vtime = -1;
  return evicted;
}

/**
 * The single block eviction would take next, without taking it.
 *
 * Shares node_chunk_score with the eviction path at chunk = 1, so to_evict and
 * evict cannot disagree about the victim. It is only a faithful preview at
 * chunk 1: with a larger chunk eviction takes a run of blocks and scores the
 * node by a different canonical block.
 */
cache_obj_t *pick_victim(cache_t *cache) {
  PartialNodeCache *impl = impl_of(cache);

  if (impl->tree.has_orphans()) {
    cache_obj_t *obj =
        hashtable_find_obj_id(cache->hashtable, impl->tree.any_orphan());
    if (obj != nullptr) return obj;
  }

  const int64_t pool = impl->tree.n_sampleable();
  if (pool == 0) return nullptr;
  const bool exhaustive = pool <= impl->n_sample;
  const int64_t draws = exhaustive ? pool : impl->n_sample;

  const PrefixRadixTree::Node *victim = nullptr;
  double best = DBL_MAX;
  for (int64_t i = 0; i < draws; ++i) {
    const PrefixRadixTree::Node *node =
        exhaustive ? impl->tree.node_at(i) : impl->tree.sample_node(next_rand());
    if (node == nullptr) break;
    double score = 0.0;
    if (!node_chunk_score(cache, impl, node, 1, score)) continue;
    if (score < best) {
      best = score;
      victim = node;
    }
  }
  return victim == nullptr ? nullptr : node_candidate(cache, impl, victim);
}

void pn_evict(cache_t *cache, const request_t *req) {
  // Single-object eviction is the n = 1 case, so there is only one place where
  // victims get chosen.
  if (pn_evict_n(cache, req, 1) == 0) {
    DEBUG_ASSERT(cache->n_obj == 0);
    WARN("no object can be evicted\n");
  }
}

bool pn_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == nullptr) return false;
  impl_of(cache)->tree.mark_evicted(obj_id);
  cache_remove_obj_base(cache, obj, true);
  return true;
}

void pn_record_request(cache_t *cache, const obj_id_t *ids, int64_t n) {
  if (ids == nullptr || n <= 0) return;
  PartialNodeCache *impl = impl_of(cache);
  impl->tree.add_sequence(ids, n);
  impl->on_record_request(ids, n);
  if (impl->tree.n_ambiguous_blocks() > 0) {
    WARN_ONCE(
        "%s: block ids are not prefix-unique, so the same block appears under "
        "more than one path and the prefix tree cannot represent it. Use "
        "prefix-hashed block ids.\n",
        cache->cache_name);
  }
}

void pn_parse_params(cache_t *cache, const char *cache_specific_params) {
  PartialNodeCache *impl = impl_of(cache);
  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;

  while (params_str != nullptr && params_str[0] != '\0') {
    char *key = strsep(&params_str, "=");
    char *value = strsep(&params_str, ",");
    while (params_str != nullptr && *params_str == ' ') ++params_str;

    if (strcasecmp(key, "n-sample") == 0) {
      impl->n_sample = static_cast<int>(strtol(value, nullptr, 0));
    } else if (strcasecmp(key, "evict-from") == 0) {
      if (strcasecmp(value, "tail") == 0) {
        impl->evict_from_tail = true;
      } else if (strcasecmp(value, "head") == 0) {
        impl->evict_from_tail = false;
      } else {
        ERROR("%s: evict-from must be tail or head, got %s\n",
              cache->cache_name, value);
      }
    } else if (strcasecmp(key, "eviction-mode") == 0) {
      if (strcasecmp(value, "drain") == 0) {
        impl->mode = PartialNodeCache::Mode::kDrain;
      } else if (strcasecmp(value, "micro") == 0) {
        impl->mode = PartialNodeCache::Mode::kMicro;
      } else {
        ERROR("%s: eviction-mode must be drain or micro, got %s\n",
              cache->cache_name, value);
      }
    } else if (strcasecmp(key, "micro-batch") == 0) {
      impl->micro_batch = strtoll(value, nullptr, 0);
    } else if (strcasecmp(key, "print") == 0) {
      printf("n-sample=%d,evict-from=%s,eviction-mode=%s,micro-batch=%lld\n",
             impl->n_sample, impl->evict_from_tail ? "tail" : "head",
             impl->mode == PartialNodeCache::Mode::kMicro ? "micro" : "drain",
             static_cast<long long>(impl->micro_batch));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s, support n-sample, evict-from, "
            "eviction-mode, micro-batch\n",
            cache->cache_name, key);
    }
  }
  free(old_params_str);
}

}  // namespace

cache_t *partial_node_cache_init(const char *cache_name,
                                 common_cache_params_t ccache_params,
                                 const char *cache_specific_params,
                                 PartialNodeCache *impl) {
  cache_t *cache =
      cache_struct_init(cache_name, ccache_params, cache_specific_params);
  cache->cache_free = pn_free;
  cache->get = pn_get;
  cache->find = pn_find;
  cache->insert = pn_insert;
  cache->evict = pn_evict;
  cache->remove = pn_remove;
  cache->to_evict = pn_to_evict;
  cache->record_request = pn_record_request;
  cache->evict_n = pn_evict_n;
  cache->obj_md_size = 0;
  cache->eviction_params = impl;

  if (cache_specific_params != nullptr) {
    pn_parse_params(cache, cache_specific_params);
  }
  return cache;
}

}  // namespace eviction
