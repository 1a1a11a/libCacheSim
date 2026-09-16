#!/usr/bin/env python3
"""
Export the real s4fifo-api LightGBM ensemble (20 models, 140400 trees) to a
compact, dependency-free binary format for libCacheSim's S4FIFO learned
control plane (S4FIFO_model_real.c).

Usage:
    python3 s4fifo_export_model.py ensemble_models.joblib cost_matrix.npy s4fifo_model.s4m

Requires: joblib, scikit-learn, lightgbm, numpy (only for running this
exporter - the resulting .s4m file has no such runtime dependency).

Format (all little-endian):
  header:
    magic            char[4]  = b"S4M1"
    n_models         uint32
    n_classes        uint32
    n_features       uint32
    n_trees_total    uint32
  per-model table (n_models entries):
    n_estimators     uint32   (trees_for_model = n_estimators * n_classes)
  per-tree table (n_trees_total entries, model-major, tree_index-minor,
                  i.e. round-major/class-minor within each model - matches
                  LightGBM's own dump_model() tree_info order):
    node_offset      uint32   (index into the global internal-node array)
    n_internal       uint32   (internal nodes in this tree; leaves = n_internal+1)
    leaf_offset      uint32   (index into the global leaf-value array)
  global internal-node array (sum of n_internal over all trees), 16 bytes each:
    feature_idx      int16    (index into the 73-feature input vector)
    left_child       int16    (local index: >=0 -> another internal node in
                               this tree; <0 -> leaf, local leaf index = -v-1)
    right_child      int16    (same convention)
    _pad             int16
    threshold        float64  (LightGBM's decision rule is always
                               "input[feature_idx] <= threshold -> left")
  global leaf-value array (sum of (n_internal+1) over all trees): float64 each
  trailer:
    cost_matrix      float64[n_classes*n_classes], row-major
"""
import os
import struct
import sys
import warnings

warnings.filterwarnings("ignore")
import joblib
import numpy as np

node_dtype = np.dtype([
    ("feature_idx", "<i2"), ("left", "<i2"), ("right", "<i2"), ("_pad", "<i2"),
    ("threshold", "<f8"),
])


def flatten_tree(root):
    """Return (nodes: list of (feat, left, right, threshold), leaves: list of float)."""
    nodes = []
    leaves = []

    def visit(node):
        if "leaf_value" in node:
            leaves.append(float(node["leaf_value"]))
            return -len(leaves)  # local leaf index = -(idx+1), i.e. leaf idx len-1
        if node["decision_type"] != "<=":
            raise ValueError(f"unsupported decision_type {node['decision_type']!r}")
        my_idx = len(nodes)
        nodes.append(None)  # placeholder, filled in after recursing
        left_ref = visit(node["left_child"])
        right_ref = visit(node["right_child"])
        nodes[my_idx] = (int(node["split_feature"]), left_ref, right_ref,
                         float(node["threshold"]))
        return my_idx

    visit(root)
    return nodes, leaves


def main():
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} ensemble_models.joblib cost_matrix.npy out.s4m",
              file=sys.stderr)
        sys.exit(1)
    model_path, cost_matrix_path, out_path = sys.argv[1:4]

    print("loading models...", file=sys.stderr)
    models = joblib.load(model_path)
    cost_matrix = np.ascontiguousarray(np.load(cost_matrix_path), dtype="<f8")
    n_models = len(models)
    n_classes = int(cost_matrix.shape[0])

    model_table = []  # n_estimators per model
    tree_table = []    # (node_offset, n_internal, leaf_offset)
    all_nodes = []     # list of (feat, left, right, threshold) tuples
    all_leaves = []    # list of float

    node_offset = 0
    leaf_offset = 0
    n_features = None

    for mi, m in enumerate(models):
        dump = m.booster_.dump_model()
        assert dump["num_class"] == n_classes, (dump["num_class"], n_classes)
        n_est = m.n_estimators
        model_table.append(n_est)
        ti = dump["tree_info"]
        assert len(ti) == n_est * n_classes, (len(ti), n_est, n_classes)
        if n_features is None:
            n_features = dump["max_feature_idx"] + 1

        for t in ti:
            assert t.get("num_cat", 0) == 0, "categorical splits not supported by this exporter"
            nodes, leaves = flatten_tree(t["tree_structure"])
            tree_table.append((node_offset, len(nodes), leaf_offset))
            all_nodes.extend(nodes)
            all_leaves.extend(leaves)
            node_offset += len(nodes)
            leaf_offset += len(leaves)

        print(f"model {mi}: n_estimators={n_est} trees={len(ti)} "
              f"cum_nodes={node_offset} cum_leaves={leaf_offset}", file=sys.stderr)

    n_trees_total = len(tree_table)

    with open(out_path, "wb") as f:
        f.write(b"S4M1")
        f.write(struct.pack("<IIII", n_models, n_classes, n_features, n_trees_total))
        for n_est in model_table:
            f.write(struct.pack("<I", n_est))
        for (no, ni, lo) in tree_table:
            f.write(struct.pack("<III", no, ni, lo))

        node_arr = np.zeros(len(all_nodes), dtype=node_dtype)
        for i, (feat, left, right, thr) in enumerate(all_nodes):
            node_arr[i] = (feat, left, right, 0, thr)
        node_arr.tofile(f)

        np.asarray(all_leaves, dtype="<f8").tofile(f)
        cost_matrix.tofile(f)

    print(f"n_models={n_models} n_classes={n_classes} n_features={n_features} "
          f"n_trees_total={n_trees_total} n_nodes={len(all_nodes)} n_leaves={len(all_leaves)}",
          file=sys.stderr)
    print(f"wrote {out_path}, size={os.path.getsize(out_path) / 1e6:.1f} MB", file=sys.stderr)


if __name__ == "__main__":
    main()
