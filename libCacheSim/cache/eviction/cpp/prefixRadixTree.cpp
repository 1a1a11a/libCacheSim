#include "prefixRadixTree.hpp"

#include <algorithm>

namespace eviction {

PrefixRadixTree::PrefixRadixTree() { root_ = new Node(); }

PrefixRadixTree::~PrefixRadixTree() { destroy(root_); }

void PrefixRadixTree::destroy(Node *node) {
  if (node == nullptr) return;
  for (auto &kv : node->children) destroy(kv.second);
  delete node;
}

PrefixRadixTree::Node *PrefixRadixTree::new_node(Node *parent,
                                                 const obj_id_t *ids,
                                                 int64_t n) {
  Node *node = new Node();
  node->parent = parent;
  node->blocks.assign(ids, ids + n);
  node->resident.assign(static_cast<size_t>(n), 0);
  for (int64_t i = 0; i < n; ++i) {
    auto it = index_.find(ids[i]);
    if (it != index_.end()) {
      // The same block id reached through a different path. Only possible when
      // block ids are not prefix-unique; the tree cannot represent that.
      ++n_ambiguous_;
      continue;
    }
    index_[ids[i]] = {node, static_cast<int32_t>(i)};
  }
  return node;
}

void PrefixRadixTree::split_node(Node *node, int64_t at) {
  const int64_t total = static_cast<int64_t>(node->blocks.size());
  if (at <= 0 || at >= total) return;

  Node *suffix = new Node();
  suffix->parent = node;
  suffix->blocks.assign(node->blocks.begin() + at, node->blocks.end());
  suffix->resident.assign(node->resident.begin() + at, node->resident.end());
  suffix->n_resident =
      std::count(suffix->resident.begin(), suffix->resident.end(),
                 static_cast<uint8_t>(1));

  // The suffix inherits the children, since they hang off the last block.
  suffix->children.swap(node->children);
  for (auto &kv : suffix->children) kv.second->parent = suffix;

  node->blocks.resize(static_cast<size_t>(at));
  node->resident.resize(static_cast<size_t>(at));
  node->n_resident = std::count(node->resident.begin(), node->resident.end(),
                                static_cast<uint8_t>(1));
  node->children.clear();
  node->children[suffix->blocks[0]] = suffix;

  // Re-home the moved blocks.
  for (size_t i = 0; i < suffix->blocks.size(); ++i) {
    auto it = index_.find(suffix->blocks[i]);
    if (it != index_.end() && it->second.first == node) {
      it->second = {suffix, static_cast<int32_t>(i)};
    }
  }

  refresh_sampleable(node);
  refresh_sampleable(suffix);
}

void PrefixRadixTree::add_sequence(const obj_id_t *ids, int64_t n) {
  Node *cur = root_;
  int64_t i = 0;

  while (i < n) {
    auto it = cur->children.find(ids[i]);
    if (it == cur->children.end()) {
      Node *child = new_node(cur, ids + i, n - i);
      cur->children[ids[i]] = child;
      return;
    }

    Node *child = it->second;
    const int64_t child_len = static_cast<int64_t>(child->blocks.size());
    int64_t j = 0;
    while (j < child_len && i + j < n && child->blocks[j] == ids[i + j]) ++j;

    if (j < child_len) {
      // The path leaves `child` partway through its run: break the run so the
      // shared part stays shared and the divergence becomes a branch.
      split_node(child, j);
    }
    i += j;
    cur = child;
  }
}

void PrefixRadixTree::refresh_sampleable(Node *node) {
  const bool want = node->n_resident > 0;
  const bool have = node->sample_idx >= 0;
  if (want == have) return;

  if (want) {
    node->sample_idx = static_cast<int64_t>(sampleable_.size());
    sampleable_.push_back(node);
  } else {
    // O(1) removal: move the last entry into the hole.
    const size_t hole = static_cast<size_t>(node->sample_idx);
    Node *last = sampleable_.back();
    sampleable_[hole] = last;
    last->sample_idx = static_cast<int64_t>(hole);
    sampleable_.pop_back();
    node->sample_idx = -1;
  }
}

void PrefixRadixTree::mark_resident(obj_id_t id) {
  auto it = index_.find(id);
  if (it == index_.end()) {
    orphans_.insert(id);
    return;
  }
  Node *node = it->second.first;
  const size_t pos = static_cast<size_t>(it->second.second);
  if (node->resident[pos] != 0) return;
  node->resident[pos] = 1;
  ++node->n_resident;
  refresh_sampleable(node);
}

bool PrefixRadixTree::mark_evicted(obj_id_t id) {
  auto it = index_.find(id);
  if (it == index_.end()) {
    orphans_.erase(id);
    return false;
  }
  Node *node = it->second.first;
  const size_t pos = static_cast<size_t>(it->second.second);
  if (node->resident[pos] == 0) return true;
  node->resident[pos] = 0;
  --node->n_resident;
  refresh_sampleable(node);
  // prune_if_empty_leaf may delete `node`; report that so callers holding the
  // pointer do not use it afterwards.
  const bool pruned = (node->n_resident == 0 && node->children.empty());
  prune_if_empty_leaf(node);
  return !pruned;
}

void PrefixRadixTree::prune_if_empty_leaf(Node *node) {
  // Only leaves are dropped. An interior node with no resident block still
  // carries the path its descendants hang from.
  while (node != root_ && node->n_resident == 0 && node->children.empty()) {
    Node *parent = node->parent;
    for (const obj_id_t id : node->blocks) {
      auto it = index_.find(id);
      if (it != index_.end() && it->second.first == node) index_.erase(it);
    }
    parent->children.erase(node->blocks[0]);
    delete node;
    node = parent;
  }
}

PrefixRadixTree::Node *PrefixRadixTree::sample_node(uint64_t rand_value) const {
  if (sampleable_.empty()) return nullptr;
  return sampleable_[rand_value % sampleable_.size()];
}

int64_t PrefixRadixTree::tail_resident(const Node *node) const {
  for (int64_t i = static_cast<int64_t>(node->resident.size()) - 1; i >= 0; --i) {
    if (node->resident[static_cast<size_t>(i)] != 0) return i;
  }
  return -1;
}

int64_t PrefixRadixTree::head_resident(const Node *node) const {
  for (size_t i = 0; i < node->resident.size(); ++i) {
    if (node->resident[i] != 0) return static_cast<int64_t>(i);
  }
  return -1;
}

int64_t PrefixRadixTree::first_resident_rank(const Node *node,
                                            bool from_tail) const {
  const int64_t total = static_cast<int64_t>(node->resident.size());
  for (int64_t k = 0; k < total; ++k) {
    const int64_t i = from_tail ? (total - 1 - k) : k;
    if (node->resident[static_cast<size_t>(i)] != 0) return k;
  }
  return -1;
}

bool PrefixRadixTree::canonical_resident(const Node *node, bool from_tail,
                                         int64_t start, int64_t lo,
                                         obj_id_t &out) const {
  const int64_t total = static_cast<int64_t>(node->resident.size());
  if (total == 0) return false;
  if (lo < 0) lo = 0;
  if (start < lo) start = lo;
  if (start > total - 1) start = total - 1;

  // Walk outwards from `start`. Checking the lower rank first at each distance
  // reproduces the tie-break of Python's stable sort on abs(x - start).
  for (int64_t d = 0; d <= total; ++d) {
    const int64_t below = start - d;
    if (below >= lo) {
      const int64_t i = from_tail ? (total - 1 - below) : below;
      if (node->resident[static_cast<size_t>(i)] != 0) {
        out = node->blocks[static_cast<size_t>(i)];
        return true;
      }
    }
    if (d == 0) continue;
    const int64_t above = start + d;
    if (above < total) {
      const int64_t i = from_tail ? (total - 1 - above) : above;
      if (node->resident[static_cast<size_t>(i)] != 0) {
        out = node->blocks[static_cast<size_t>(i)];
        return true;
      }
    }
  }
  return false;
}

void PrefixRadixTree::collect_resident(const Node *node, bool from_tail,
                                       int64_t max_take,
                                       std::vector<obj_id_t> &out) const {
  const int64_t total = static_cast<int64_t>(node->resident.size());
  for (int64_t k = 0; k < total && static_cast<int64_t>(out.size()) < max_take;
       ++k) {
    const int64_t i = from_tail ? (total - 1 - k) : k;
    if (node->resident[static_cast<size_t>(i)] == 0) continue;
    out.push_back(node->blocks[static_cast<size_t>(i)]);
  }
}

}  // namespace eviction
