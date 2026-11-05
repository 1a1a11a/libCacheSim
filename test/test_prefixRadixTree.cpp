// Checks for the prefix tree behind the partial-node policies
// (cache/eviction/cpp/prefixRadixTree.hpp), driven directly rather than
// through a simulator.

#include <cstdint>
#include <cstdio>

#include "cache/eviction/cpp/prefixRadixTree.hpp"

namespace {

/// The partial-node prefix tree must not keep a block both in a node and among
/// its orphans. This is how partial_node_random_compute_small_queue got one: a
/// block waiting in the small queue is not resident in the tree, so its leaf
/// is pruned when the leaf's last resident block goes. Promoted afterwards, the
/// block has no node and becomes an orphan. A later request on the same path
/// re-creates the node. If the orphan entry survived that, evicting orphans
/// drew it forever: one eviction removed it from the cache but not from the
/// orphans, and every later draw found nothing to evict.
bool orphan_rejoins_its_path() {
  eviction::PrefixRadixTree tree;
  const obj_id_t path[] = {1, 2};
  tree.add_sequence(path, 2);
  tree.mark_resident(1);  // block 2 waits in the small queue
  tree.mark_evicted(1);   // the leaf has nothing resident left: pruned
  tree.mark_resident(2);  // 2 is promoted, with no node to go to
  const bool orphaned = tree.has_orphans();
  tree.add_sequence(path, 2);  // the path is requested again

  bool ok = true;
  if (!orphaned) {
    printf("  FAIL: block 2 was not an orphan after its leaf was pruned\n");
    ok = false;
  }
  if (tree.has_orphans() || !tree.is_resident(2) || tree.is_resident(1) ||
      tree.n_sampleable() != 1) {
    printf("  FAIL: re-recording the path did not take block 2 back in\n");
    ok = false;
  }
  tree.mark_evicted(2);
  if (tree.has_orphans() || tree.n_sampleable() != 0) {
    printf("  FAIL: evicting block 2 left it behind\n");
    ok = false;
  }
  return ok;
}

}  // namespace

int main(void) {
  printf("a promoted orphan rejoins its path\n");
  if (!orphan_rejoins_its_path()) {
    printf("prefix tree check FAILED\n");
    return 1;
  }
  printf("all prefix tree checks passed\n");
  return 0;
}
