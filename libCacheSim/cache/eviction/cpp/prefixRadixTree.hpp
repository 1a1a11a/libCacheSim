// A path-compressed prefix (radix) tree over cache block ids.
//
// A request is a path from the root: block 0 is the child of the root, block i
// the child of block i-1. Requests that share a prompt share that path, so the
// tree makes explicit the structure a flat hash table throws away.
//
// A *node* is a maximal non-branching run of blocks. Eviction algorithms that
// work at node granularity sample nodes and then evict part of one -- hence
// "partial-node" eviction. This mirrors the RadixTree behind
// RadixTreeFreeBlockManager in the vLLM prototype.
//
// Residency is tracked explicitly rather than by probing the cache: the tree is
// told about every insert and every eviction, so it always knows which of a
// node's blocks are actually in the cache. That is what makes finding a node's
// eviction candidate O(1) amortised instead of a scan plus a hash lookup per
// block.

#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" {
#include "libCacheSim/cacheObj.h"
}

namespace eviction {

class PrefixRadixTree {
 public:
  struct Node {
    /// Blocks of this run, root-ward first. blocks[i+1] is the child of
    /// blocks[i]; children hang off the last block.
    std::vector<obj_id_t> blocks;
    /// Parallel to blocks: is this block currently in the cache?
    std::vector<uint8_t> resident;
    int64_t n_resident = 0;

    Node *parent = nullptr;
    /// Keyed by the child node's first block id.
    std::unordered_map<obj_id_t, Node *> children;

    /// Index into the sampleable array, or -1 when this node has no resident
    /// block and is therefore not a useful eviction candidate.
    int64_t sample_idx = -1;
  };

  PrefixRadixTree();
  ~PrefixRadixTree();

  PrefixRadixTree(const PrefixRadixTree &) = delete;
  PrefixRadixTree &operator=(const PrefixRadixTree &) = delete;

  /// Add the path ids[0..n-1], splitting nodes where it diverges from an
  /// existing path. Blocks are added as non-resident; mark_resident() promotes
  /// them when they actually enter the cache.
  void add_sequence(const obj_id_t *ids, int64_t n);

  /// A block entered the cache. Blocks the tree has never seen are remembered
  /// as orphans so eviction can still reach them.
  void mark_resident(obj_id_t id);

  /// A block left the cache. Prunes leaf nodes that have become empty.
  ///
  /// Returns false when the owning node was pruned by this call (or the block
  /// was an orphan), i.e. when any Node* the caller still holds for it is now
  /// dangling. Callers that keep node pointers across an eviction MUST check.
  bool mark_evicted(obj_id_t id);

  /// Uniformly pick a node holding at least one resident block, or nullptr.
  Node *sample_node(uint64_t rand_value) const;

  /// Deepest / shallowest resident block position in `node`; -1 if none.
  int64_t tail_resident(const Node *node) const;
  int64_t head_resident(const Node *node) const;

  /// Rank of the first resident block in eviction take order, or -1.
  int64_t first_resident_rank(const Node *node, bool from_tail) const;

  /// The resident block nearest rank `start` in eviction take order, scanning
  /// outwards and never below rank `lo`; ties go to the lower rank.
  ///
  /// This is vLLM's canonical-block rule
  /// (_get_canonical_block_for_radix_tree_node / _get_chunk_canonical_block),
  /// expressed over take-order ranks rather than raw positions so it still holds
  /// when eviction runs tail-first. For the default head-first order rank equals
  /// position and the two are identical.
  bool canonical_resident(const Node *node, bool from_tail, int64_t start,
                          int64_t lo, obj_id_t &out) const;

  /// The first `max_take` resident blocks in eviction take order. Snapshotting
  /// them up front matters: evicting the last one can prune the node, after
  /// which the Node* is dangling.
  void collect_resident(const Node *node, bool from_tail, int64_t max_take,
                        std::vector<obj_id_t> &out) const;

  int64_t n_sampleable() const { return static_cast<int64_t>(sampleable_.size()); }
  /// Direct access to the candidate pool, for scanning it exhaustively when it
  /// is smaller than the sample size. Valid indices are [0, n_sampleable).
  Node *node_at(int64_t i) const { return sampleable_[static_cast<size_t>(i)]; }
  bool has_orphans() const { return !orphans_.empty(); }
  obj_id_t any_orphan() const { return *orphans_.begin(); }

  /// True if a block id was seen under a path different from the one it is
  /// already registered under. Non-zero means the trace's block ids are not
  /// prefix-unique and the tree cannot be trusted.
  int64_t n_ambiguous_blocks() const { return n_ambiguous_; }

 private:
  Node *new_node(Node *parent, const obj_id_t *ids, int64_t n);
  /// Split `node` so it keeps blocks[0..at-1] and a fresh child holds the rest.
  void split_node(Node *node, int64_t at);
  void refresh_sampleable(Node *node);
  void prune_if_empty_leaf(Node *node);
  void destroy(Node *node);

  Node *root_;
  /// block id -> (owning node, position within that node)
  std::unordered_map<obj_id_t, std::pair<Node *, int32_t>> index_;
  /// Nodes with at least one resident block, so every sample is productive.
  std::vector<Node *> sampleable_;
  /// Resident blocks with no node, e.g. inserted without a recorded request.
  std::unordered_set<obj_id_t> orphans_;
  int64_t n_ambiguous_ = 0;
};

}  // namespace eviction
