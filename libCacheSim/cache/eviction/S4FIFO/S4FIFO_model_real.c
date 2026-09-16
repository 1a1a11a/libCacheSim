//
// S4FIFO_model_real.c - see S4FIFO_model_real.h.
//
// Binary format written by scripts/s4fifo_export_model.py, and transcribed
// verbatim into C by scripts/s4fifo_embed_model.py for the compiled-in
// default model (model.h):
//
// clang-format off
//   magic            char[4]  = "S4M1"
//   n_models         uint32
//   n_classes        uint32
//   n_features       uint32
//   n_trees_total    uint32
//   n_estimators[n_models]                                      uint32 each
//   tree_table[n_trees_total]: {node_offset, n_internal, leaf_offset}   uint32 x3 each
//   nodes[sum n_internal]: {feature_idx, left, right, _pad, threshold}  int16 x4 + double
//   leaves[sum (n_internal+1)]                                         double each
//   cost_matrix[n_classes*n_classes], row-major                        double each
// clang-format on
//
// A tree's nodes/leaves live at nodes[node_offset .. node_offset+n_internal)
// and leaves[leaf_offset .. leaf_offset+n_internal+1). Within a tree, child
// indices are LOCAL (0-based into that tree's own node range): a
// non-negative child continues at that local node index; a negative child
// c means leaf index (-c - 1). A tree with n_internal == 0 is a single bare
// leaf, leaves[leaf_offset].
//

#include "S4FIFO_model_real.h"

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "S4FIFO_predictor.h"
#include "libCacheSim/logging.h"

#ifdef __cplusplus
extern "C" {
#endif

#define S4FIFO_REAL_MODEL_MAX_CLASSES 64

typedef struct {
  int16_t feature_idx;
  int16_t left;
  int16_t right;
  int16_t _pad;
  double threshold;
} s4fifo_real_node_t;

typedef struct {
  uint32_t node_offset;
  uint32_t n_internal;
  uint32_t leaf_offset;
} s4fifo_real_tree_t;

// The on-disk layout (scripts/s4fifo_export_model.py) assumes these exact,
// unpadded sizes so a whole array can be fread() directly.
_Static_assert(sizeof(s4fifo_real_node_t) == 16,
               "s4fifo_real_node_t must be 16 bytes");
_Static_assert(sizeof(s4fifo_real_tree_t) == 12,
               "s4fifo_real_tree_t must be 12 bytes");

// const pointers so one struct describes either a model read from a file
// or the compiled-in default's static arrays.
typedef struct {
  uint32_t n_models;
  uint32_t n_classes;
  uint32_t n_features;
  uint32_t n_trees_total;
  const uint32_t *n_estimators;     // [n_models]
  const s4fifo_real_tree_t *trees;  // [n_trees_total]
  const s4fifo_real_node_t *nodes;  // [total internal nodes]
  const double *leaves;             // [total leaves]
  const double *cost_matrix;        // [n_classes * n_classes], row-major
} s4fifo_real_model_t;

// The compiled-in default, used when no `model-path=` was given or it
// failed to load. Same arrays and layout as a .s4m, so it runs through the
// same evaluator and predicts identically.
#include "model.h"

static const s4fifo_real_model_t g_embedded_model = {
    .n_models = S4FIFO_EMBEDDED_N_MODELS,
    .n_classes = S4FIFO_EMBEDDED_N_CLASSES,
    .n_features = S4FIFO_EMBEDDED_N_FEATURES,
    .n_trees_total = S4FIFO_EMBEDDED_N_TREES,
    .n_estimators = kS4FIFOEmbeddedEstimators,
    .trees = kS4FIFOEmbeddedTrees,
    .nodes = kS4FIFOEmbeddedNodes,
    .leaves = kS4FIFOEmbeddedLeaves,
    .cost_matrix = kS4FIFOEmbeddedCost,
};

static pthread_mutex_t g_load_mutex = PTHREAD_MUTEX_INITIALIZER;
static s4fifo_real_model_t *g_model = NULL;  // NULL until a load succeeds
static bool g_load_attempted =
    false;  // true after the first attempt, success or not

static bool read_exact(FILE *fp, void *buf, size_t n) {
  return fread(buf, 1, n, fp) == n;
}

static s4fifo_real_model_t *load_model_file(const char *path) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    WARN("S4FIFO: cannot open model file %s\n", path);
    return NULL;
  }

  s4fifo_real_model_t *model = calloc(1, sizeof(s4fifo_real_model_t));
  // read through non-const pointers, handed over once everything is in, so
  // the failure path frees plain `void *`s with no const-casting
  uint32_t *n_estimators = NULL;
  s4fifo_real_tree_t *trees = NULL;
  s4fifo_real_node_t *nodes = NULL;
  double *leaves = NULL;
  double *cost_matrix = NULL;
  bool ok = false;

  char magic[4];
  uint32_t header[4];
  if (!read_exact(fp, magic, 4) || memcmp(magic, "S4M1", 4) != 0 ||
      !read_exact(fp, header, sizeof(header))) {
    WARN("S4FIFO: %s is not a valid S4M1 model file\n", path);
    goto out;
  }
  model->n_models = header[0];
  model->n_classes = header[1];
  model->n_features = header[2];
  model->n_trees_total = header[3];

  if (model->n_classes == 0 ||
      model->n_classes > S4FIFO_REAL_MODEL_MAX_CLASSES ||
      model->n_models == 0 || model->n_trees_total == 0) {
    WARN(
        "S4FIFO: %s has implausible header (n_models=%u n_classes=%u "
        "n_trees_total=%u)\n",
        path, model->n_models, model->n_classes, model->n_trees_total);
    goto out;
  }

  n_estimators = malloc(model->n_models * sizeof(uint32_t));
  if (!read_exact(fp, n_estimators, model->n_models * sizeof(uint32_t))) {
    WARN("S4FIFO: %s truncated (n_estimators table)\n", path);
    goto out;
  }

  trees = malloc(model->n_trees_total * sizeof(s4fifo_real_tree_t));
  if (!read_exact(fp, trees,
                  model->n_trees_total * sizeof(s4fifo_real_tree_t))) {
    WARN("S4FIFO: %s truncated (tree table)\n", path);
    goto out;
  }

  const s4fifo_real_tree_t *last = &trees[model->n_trees_total - 1];
  uint64_t n_nodes = (uint64_t)last->node_offset + last->n_internal;
  uint64_t n_leaves = (uint64_t)last->leaf_offset + last->n_internal + 1;

  nodes = malloc(n_nodes * sizeof(s4fifo_real_node_t));
  if (!read_exact(fp, nodes, n_nodes * sizeof(s4fifo_real_node_t))) {
    WARN("S4FIFO: %s truncated (node array)\n", path);
    goto out;
  }

  leaves = malloc(n_leaves * sizeof(double));
  if (!read_exact(fp, leaves, n_leaves * sizeof(double))) {
    WARN("S4FIFO: %s truncated (leaf array)\n", path);
    goto out;
  }

  cost_matrix = malloc(model->n_classes * model->n_classes * sizeof(double));
  if (!read_exact(fp, cost_matrix,
                  model->n_classes * model->n_classes * sizeof(double))) {
    WARN("S4FIFO: %s truncated (cost matrix)\n", path);
    goto out;
  }

  model->n_estimators = n_estimators;
  model->trees = trees;
  model->nodes = nodes;
  model->leaves = leaves;
  model->cost_matrix = cost_matrix;

  INFO("S4FIFO: loaded real model %s (%u models, %u trees, %llu nodes)\n", path,
       model->n_models, model->n_trees_total, (unsigned long long)n_nodes);
  ok = true;

out:
  fclose(fp);
  if (!ok) {
    free(n_estimators);
    free(trees);
    free(nodes);
    free(leaves);
    free(cost_matrix);
    free(model);
    return NULL;
  }
  return model;
}

bool s4fifo_real_model_load(const char *path) {
  pthread_mutex_lock(&g_load_mutex);
  if (!g_load_attempted) {
    g_model = load_model_file(path);
    g_load_attempted = true;
  }
  bool loaded = (g_model != NULL);
  pthread_mutex_unlock(&g_load_mutex);
  return loaded;
}

// Evaluate one tree, returning its raw (pre-softmax) contribution.
static double eval_tree(const s4fifo_real_model_t *model, uint32_t tree_idx,
                        const double *input) {
  const s4fifo_real_tree_t *t = &model->trees[tree_idx];
  if (t->n_internal == 0) {
    return model->leaves[t->leaf_offset];
  }

  int32_t local = 0;
  for (;;) {
    const s4fifo_real_node_t *n = &model->nodes[t->node_offset + local];
    int16_t child =
        (input[n->feature_idx] <= n->threshold) ? n->left : n->right;
    if (child < 0) {
      uint32_t leaf_local = (uint32_t)(-child - 1);
      return model->leaves[t->leaf_offset + leaf_local];
    }
    local = child;
  }
}

bool s4fifo_real_model_predict(const double *input73, S4FIFOConfigEntry *out) {
  pthread_mutex_lock(&g_load_mutex);
  const s4fifo_real_model_t *model = g_model;
  pthread_mutex_unlock(&g_load_mutex);
  if (model == NULL) model = &g_embedded_model;

  double avg_probs[S4FIFO_REAL_MODEL_MAX_CLASSES] = {0};
  double raw[S4FIFO_REAL_MODEL_MAX_CLASSES];
  double probs[S4FIFO_REAL_MODEL_MAX_CLASSES];
  uint32_t n_classes = model->n_classes;
  uint32_t tree_idx = 0;

  for (uint32_t m = 0; m < model->n_models; m++) {
    memset(raw, 0, n_classes * sizeof(double));
    uint32_t n_trees_this_model = model->n_estimators[m] * n_classes;
    for (uint32_t t = 0; t < n_trees_this_model; t++) {
      raw[t % n_classes] += eval_tree(model, tree_idx, input73);
      tree_idx++;
    }

    double max_v = raw[0];
    for (uint32_t c = 1; c < n_classes; c++) {
      if (raw[c] > max_v) max_v = raw[c];
    }
    double sum = 0.0;
    for (uint32_t c = 0; c < n_classes; c++) {
      probs[c] = exp(raw[c] - max_v);
      sum += probs[c];
    }
    for (uint32_t c = 0; c < n_classes; c++) {
      avg_probs[c] += probs[c] / sum;
    }
  }
  for (uint32_t c = 0; c < n_classes; c++) {
    avg_probs[c] /= model->n_models;
  }

  // Minimum expected risk selection (matches predictor.py exactly):
  // risk[c] = sum_c2 cost_matrix[c, c2] * avg_probs[c2]; pick argmin(risk).
  double best_risk = 0.0;
  int best_class = -1;
  for (uint32_t c = 0; c < n_classes; c++) {
    double risk = 0.0;
    for (uint32_t c2 = 0; c2 < n_classes; c2++) {
      risk += model->cost_matrix[c * n_classes + c2] * avg_probs[c2];
    }
    if (best_class < 0 || risk < best_risk) {
      best_risk = risk;
      best_class = (int)c;
    }
  }

  if (best_class < 0 || (uint32_t)best_class >= S4FIFO_MODEL_N_CONFIGS) {
    return false;
  }
  *out = s4fifo_get_config_table()[best_class];
  return true;
}

#ifdef __cplusplus
}
#endif
