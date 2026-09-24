//
// S4FIFO_features.h - O(1) cache-level feature collection for S4FIFO's
// optional learned control plane (see S4FIFO_predictor.h).
//
// Tracks, per sub-queue (small/main/ghost), a histogram of "hit position"
// (how many other inserts happened between an object's insertion and its
// next hit) using an O(1) bucketed counter scheme keyed by insertion
// sequence number, plus simple hit/miss/unique counters. Calling code (in
// S4FIFO.c) is responsible for only invoking the record_* functions while
// in the FEATURE_COLLECT phase.
//
// Ported/adapted from williamnixon20/CacheLib@72bb8103
// cachelib/allocator/MMS4FIFO.h's S4FIFOHitPosTracker/S4FIFOFeatureCollector,
// which is what the lite model was trained against - the field
// semantics here (e.g. total_hits including ghost hits) must match that
// exactly for predictions to be meaningful.
//

#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define S4FIFO_FEATURE_MAX_BUCKETS 64
#define S4FIFO_FEATURE_DEFAULT_BUCKETS 20

// ============================================================================
// Bucketed hit-position tracker
// ============================================================================

typedef struct {
  int32_t num_buckets;
  int64_t bucket_size;

  int64_t hit_counts[S4FIFO_FEATURE_MAX_BUCKETS];
  int64_t total_hits;

  // Ghost queue only: track how many objects were removed from a bucket
  // since it was last visited, so hit-position estimates can subtract out
  // the "holes" left by objects that were evicted from the ghost queue
  // before a stale hit on an older insert could be recorded against them.
  bool track_middle_removal;
  int64_t removal_counters[S4FIFO_FEATURE_MAX_BUCKETS];
  int64_t current_bucket;
} S4FIFO_hit_pos_tracker_t;

static inline void S4FIFO_hit_pos_tracker_init(S4FIFO_hit_pos_tracker_t *t,
                                               int64_t expected_max_pos,
                                               int32_t num_buckets,
                                               bool track_middle_removal) {
  memset(t, 0, sizeof(*t));
  if (num_buckets <= 0) num_buckets = S4FIFO_FEATURE_DEFAULT_BUCKETS;
  if (num_buckets > S4FIFO_FEATURE_MAX_BUCKETS)
    num_buckets = S4FIFO_FEATURE_MAX_BUCKETS;
  t->num_buckets = num_buckets;
  int64_t bucket_size = (expected_max_pos + num_buckets - 1) / num_buckets;
  t->bucket_size = bucket_size > 1 ? bucket_size : 1;
  t->track_middle_removal = track_middle_removal;
}

// Records that an insert just happened (at the queue's own monotonically
// increasing insert-sequence-number counter) and returns the bucket it
// landed in - callers store this alongside the object so a later hit can
// be attributed to the right bucket.
static inline int64_t S4FIFO_hit_pos_tracker_record_insert(
    S4FIFO_hit_pos_tracker_t *t, int64_t insert_counter) {
  int64_t new_bucket = (insert_counter / t->bucket_size) % t->num_buckets;
  if (t->track_middle_removal && new_bucket != t->current_bucket) {
    int64_t b = (t->current_bucket + 1) % t->num_buckets;
    while (b != new_bucket) {
      t->removal_counters[b] = 0;
      b = (b + 1) % t->num_buckets;
    }
    t->removal_counters[new_bucket] = 0;
  }
  t->current_bucket = new_bucket;
  return new_bucket;
}

static inline void S4FIFO_hit_pos_tracker_record_removal(
    S4FIFO_hit_pos_tracker_t *t) {
  if (!t->track_middle_removal) return;
  t->removal_counters[t->current_bucket]++;
}

static inline int64_t S4FIFO_hit_pos_tracker_estimate_holes(
    const S4FIFO_hit_pos_tracker_t *t, int64_t insert_bucket) {
  if (!t->track_middle_removal) return 0;
  int64_t holes = 0;
  int64_t bucket = insert_bucket;
  while (bucket != t->current_bucket) {
    holes += t->removal_counters[bucket];
    bucket = (bucket + 1) % t->num_buckets;
  }
  holes += t->removal_counters[t->current_bucket];
  return holes;
}

static inline void S4FIFO_hit_pos_tracker_record_hit(
    S4FIFO_hit_pos_tracker_t *t, int64_t insert_time, int64_t insert_bucket,
    int64_t current_counter) {
  int64_t raw_position = current_counter - insert_time;
  int64_t holes = S4FIFO_hit_pos_tracker_estimate_holes(t, insert_bucket);
  int64_t adjusted_position = raw_position - holes;
  if (adjusted_position < 0) adjusted_position = 0;
  int64_t pos_bucket = adjusted_position / t->bucket_size;
  if (pos_bucket >= t->num_buckets) pos_bucket = t->num_buckets - 1;
  t->hit_counts[pos_bucket]++;
  t->total_hits++;
}

static inline void S4FIFO_hit_pos_tracker_get_histogram(
    const S4FIFO_hit_pos_tracker_t *t, double *out /* [num_buckets] */) {
  if (t->total_hits == 0) {
    memset(out, 0, t->num_buckets * sizeof(double));
    return;
  }
  double inv_total = 1.0 / (double)t->total_hits;
  for (int i = 0; i < t->num_buckets; i++) {
    out[i] = (double)t->hit_counts[i] * inv_total;
  }
}

// ============================================================================
// Feature vector (input to S4FIFO_predictor.h)
// ============================================================================

typedef struct {
  int32_t num_buckets;
  double log_cache_capacity;

  double hit_ratio_small;  // H_s
  double hit_ratio_main;   // H_m
  double hit_ratio_ghost;  // H_g

  double unique_ratio;   // rho_unique
  double one_hit_ratio;  // rho_onehit

  int64_t total_requests;
  int64_t total_hits;  // hits_small + hits_main + hits_ghost
  int64_t hits_small;
  int64_t hits_main;
  int64_t hits_ghost;

  double hist_small[S4FIFO_FEATURE_MAX_BUCKETS];
  double hist_main[S4FIFO_FEATURE_MAX_BUCKETS];
  double hist_ghost[S4FIFO_FEATURE_MAX_BUCKETS];

  // Trend features: (second-half - first-half) of the observation window,
  // split at its midpoint (in request count). Everything above is a single
  // aggregate over the whole window and cannot tell a workload that is
  // stable throughout from one that is, say, ramping into a scan or cooling
  // off - these three cheap deltas (reusing counters already being tracked,
  // just snapshotted once at the midpoint) are the minimal signal for "is
  // the workload changing during the window itself". 0.0 if the window was
  // too short for a midpoint to be reached (see midpoint_captured below).
  double hit_ratio_trend;     // second-half overall hit ratio - first-half
  double unique_ratio_trend;  // second-half unique-rate - first-half
  double small_share_trend;  // second-half (hits_small/total_hits) - first-half
} S4FIFO_feature_vector_t;

// ============================================================================
// Feature collector
//
// This is intentionally a "dumb" bucketing utility: it does not track its
// own insert-sequence counters. S4FIFO.c already owns a small-FIFO insert
// counter (needed by the base algorithm's skip-ratio check regardless of
// whether feature collection is enabled) and, when this module is in use,
// owns matching counters for the main and ghost FIFOs too - callers pass
// the current counter value into record_insert_*/record_hit_* directly, so
// there is exactly one source of truth for each queue's insert sequence.
// ============================================================================

typedef struct {
  int64_t cache_capacity;
  int32_t num_buckets;

  S4FIFO_hit_pos_tracker_t small_tracker;
  S4FIFO_hit_pos_tracker_t main_tracker;
  S4FIFO_hit_pos_tracker_t ghost_tracker;

  int64_t hits_small;
  int64_t hits_main;
  int64_t hits_ghost;
  int64_t total_requests;
  int64_t total_unique;
  int64_t one_hit_count;

  // Midpoint snapshot for trend features (see S4FIFO_feature_vector_t):
  // taken once, the first time total_requests reaches midpoint_reqs.
  // midpoint_reqs <= 0 disables trend tracking entirely (e.g. caller
  // doesn't know the window size ahead of time).
  int64_t midpoint_reqs;
  bool midpoint_captured;
  int64_t mid_total_requests;
  int64_t mid_hits_small;
  int64_t mid_hits_main;
  int64_t mid_hits_ghost;
  int64_t mid_total_unique;
} S4FIFO_feature_collector_t;

static inline void S4FIFO_feature_collector_init(
    S4FIFO_feature_collector_t *fc, int64_t cache_capacity, int64_t small_size,
    int64_t main_size, int64_t ghost_size, int32_t num_buckets,
    int64_t midpoint_reqs) {
  memset(fc, 0, sizeof(*fc));
  fc->cache_capacity = cache_capacity;
  fc->num_buckets =
      num_buckets > 0 ? num_buckets : S4FIFO_FEATURE_DEFAULT_BUCKETS;
  if (fc->num_buckets > S4FIFO_FEATURE_MAX_BUCKETS)
    fc->num_buckets = S4FIFO_FEATURE_MAX_BUCKETS;
  S4FIFO_hit_pos_tracker_init(&fc->small_tracker, small_size, fc->num_buckets,
                              false);
  S4FIFO_hit_pos_tracker_init(&fc->main_tracker, main_size, fc->num_buckets,
                              false);
  S4FIFO_hit_pos_tracker_init(&fc->ghost_tracker, ghost_size, fc->num_buckets,
                              true);
  fc->midpoint_reqs = midpoint_reqs;
}

// Each record_insert_*/record_hit_* pair below takes the CURRENT value of
// the caller-owned insert-sequence counter for that queue (small/main:
// irrelevant beyond bucketing, so any monotonically increasing counter
// works; ghost: must be the same counter used when the object was
// originally inserted, so hole estimation lines up).

static inline void S4FIFO_feature_collector_record_insert_small(
    S4FIFO_feature_collector_t *fc, int64_t small_insert_counter) {
  S4FIFO_hit_pos_tracker_record_insert(&fc->small_tracker,
                                       small_insert_counter);
}

static inline void S4FIFO_feature_collector_record_insert_main(
    S4FIFO_feature_collector_t *fc, int64_t main_insert_counter) {
  S4FIFO_hit_pos_tracker_record_insert(&fc->main_tracker, main_insert_counter);
}

static inline int32_t S4FIFO_feature_collector_record_insert_ghost(
    S4FIFO_feature_collector_t *fc, int64_t ghost_insert_counter) {
  return (int32_t)S4FIFO_hit_pos_tracker_record_insert(&fc->ghost_tracker,
                                                       ghost_insert_counter);
}

static inline void S4FIFO_feature_collector_record_ghost_removal(
    S4FIFO_feature_collector_t *fc) {
  S4FIFO_hit_pos_tracker_record_removal(&fc->ghost_tracker);
}

static inline void S4FIFO_feature_collector_record_hit_small(
    S4FIFO_feature_collector_t *fc, int64_t insert_time,
    int64_t small_insert_counter) {
  fc->hits_small++;
  S4FIFO_hit_pos_tracker_record_hit(&fc->small_tracker, insert_time, 0,
                                    small_insert_counter);
}

static inline void S4FIFO_feature_collector_record_hit_main(
    S4FIFO_feature_collector_t *fc, int64_t insert_time,
    int64_t main_insert_counter) {
  fc->hits_main++;
  S4FIFO_hit_pos_tracker_record_hit(&fc->main_tracker, insert_time, 0,
                                    main_insert_counter);
}

static inline void S4FIFO_feature_collector_record_hit_ghost(
    S4FIFO_feature_collector_t *fc, int64_t insert_time, int32_t insert_bucket,
    int64_t ghost_insert_counter) {
  fc->hits_ghost++;
  S4FIFO_hit_pos_tracker_record_hit(&fc->ghost_tracker, insert_time,
                                    insert_bucket, ghost_insert_counter);
}

static inline void S4FIFO_feature_collector_record_request(
    S4FIFO_feature_collector_t *fc) {
  fc->total_requests++;
  // Snapshot cumulative counters the first time we cross the midpoint, so
  // get_features() can later derive first-half-vs-second-half trends. This
  // runs before the current request's own hit/miss is recorded (record_request
  // is called before cache_get_base() in S4FIFO_get()), so the snapshot is
  // off by at most one request - irrelevant for a window of thousands.
  if (!fc->midpoint_captured && fc->midpoint_reqs > 0 &&
      fc->total_requests >= fc->midpoint_reqs) {
    fc->midpoint_captured = true;
    fc->mid_total_requests = fc->total_requests;
    fc->mid_hits_small = fc->hits_small;
    fc->mid_hits_main = fc->hits_main;
    fc->mid_hits_ghost = fc->hits_ghost;
    fc->mid_total_unique = fc->total_unique;
  }
}

static inline void S4FIFO_feature_collector_record_unique(
    S4FIFO_feature_collector_t *fc) {
  fc->total_unique++;
}

static inline void S4FIFO_feature_collector_record_one_hit(
    S4FIFO_feature_collector_t *fc) {
  fc->one_hit_count++;
}

static inline void S4FIFO_feature_collector_get_features(
    const S4FIFO_feature_collector_t *fc, S4FIFO_feature_vector_t *fv) {
  fv->num_buckets = fc->num_buckets;
  fv->log_cache_capacity =
      fc->cache_capacity > 0 ? log10((double)fc->cache_capacity) : 0.0;

  fv->total_requests = fc->total_requests;
  fv->hits_small = fc->hits_small;
  fv->hits_main = fc->hits_main;
  fv->hits_ghost = fc->hits_ghost;
  fv->total_hits = fc->hits_small + fc->hits_main + fc->hits_ghost;

  if (fv->total_hits > 0) {
    fv->hit_ratio_small = (double)fc->hits_small / fv->total_hits;
    fv->hit_ratio_main = (double)fc->hits_main / fv->total_hits;
    fv->hit_ratio_ghost = (double)fc->hits_ghost / fv->total_hits;
  } else {
    fv->hit_ratio_small = 0.0;
    fv->hit_ratio_main = 0.0;
    fv->hit_ratio_ghost = 0.0;
  }

  fv->unique_ratio = fc->total_requests > 0
                         ? (double)fc->total_unique / (double)fc->total_requests
                         : 0.0;
  fv->one_hit_ratio = fc->total_unique > 0
                          ? (double)fc->one_hit_count / (double)fc->total_unique
                          : 0.0;

  S4FIFO_hit_pos_tracker_get_histogram(&fc->small_tracker, fv->hist_small);
  S4FIFO_hit_pos_tracker_get_histogram(&fc->main_tracker, fv->hist_main);
  S4FIFO_hit_pos_tracker_get_histogram(&fc->ghost_tracker, fv->hist_ghost);

  if (fc->midpoint_captured) {
    int64_t fh_requests = fc->mid_total_requests;
    int64_t fh_hits_small = fc->mid_hits_small;
    int64_t fh_hits_main = fc->mid_hits_main;
    int64_t fh_hits_ghost = fc->mid_hits_ghost;
    int64_t fh_unique = fc->mid_total_unique;
    int64_t fh_total_hits = fh_hits_small + fh_hits_main + fh_hits_ghost;

    int64_t sh_requests = fc->total_requests - fc->mid_total_requests;
    int64_t sh_hits_small = fc->hits_small - fc->mid_hits_small;
    int64_t sh_hits_main = fc->hits_main - fc->mid_hits_main;
    int64_t sh_hits_ghost = fc->hits_ghost - fc->mid_hits_ghost;
    int64_t sh_unique = fc->total_unique - fc->mid_total_unique;
    int64_t sh_total_hits = sh_hits_small + sh_hits_main + sh_hits_ghost;

    double fh_hit_ratio =
        fh_requests > 0 ? (double)fh_total_hits / fh_requests : 0.0;
    double sh_hit_ratio =
        sh_requests > 0 ? (double)sh_total_hits / sh_requests : 0.0;
    fv->hit_ratio_trend = sh_hit_ratio - fh_hit_ratio;

    double fh_unique_ratio =
        fh_requests > 0 ? (double)fh_unique / fh_requests : 0.0;
    double sh_unique_ratio =
        sh_requests > 0 ? (double)sh_unique / sh_requests : 0.0;
    fv->unique_ratio_trend = sh_unique_ratio - fh_unique_ratio;

    double fh_small_share =
        fh_total_hits > 0 ? (double)fh_hits_small / fh_total_hits : 0.0;
    double sh_small_share =
        sh_total_hits > 0 ? (double)sh_hits_small / sh_total_hits : 0.0;
    fv->small_share_trend = sh_small_share - fh_small_share;
  } else {
    fv->hit_ratio_trend = 0.0;
    fv->unique_ratio_trend = 0.0;
    fv->small_share_trend = 0.0;
  }
}

#ifdef __cplusplus
}
#endif
