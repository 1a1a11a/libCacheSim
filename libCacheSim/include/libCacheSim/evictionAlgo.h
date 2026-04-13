#pragma once

#include "cache.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;
} FIFO_params_t;

/* used by LFU related */
typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  int64_t n_obj_promoted;
  int64_t n_byte_promoted;
} LRU_params_t;

/* used by LFU related */
typedef struct freq_node {
  int64_t freq;
  cache_obj_t *first_obj;
  cache_obj_t *last_obj;
  int32_t n_obj;
} freq_node_t;

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;
  // clock uses one-bit counter
  int32_t n_bit_counter;
  // max_freq = 1 << (n_bit_counter - 1)
  int32_t max_freq;
  int32_t init_freq;

  int64_t n_obj_rewritten;
  int64_t n_byte_rewritten;

  int64_t n_miss;

  // tracking stats for hand position / retention ratio plotting
  int64_t n_obj_examined_interval;
  int64_t n_obj_retained_interval;
  int64_t n_evictions_interval;
  FILE *tracking_file;
  int64_t last_report_vtime;
} Clock_params_t;

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  // points to the eviction position
  cache_obj_t *next_to_merge;
  // the number of objects to examine at each eviction
  int n_exam_obj;
  // reinsertion ratio: fraction of examined objects to reinsert
  double reinsertion_ratio;

  int64_t n_obj_rewritten;
  int64_t n_byte_rewritten;
} ClockRI_params_t;

typedef struct {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  int64_t n_miss;

  int64_t n_obj_rewritten;
  int64_t n_byte_rewritten;
} ClockOracle_params_t;

cache_t *ARC_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

cache_t *ARCv0_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

cache_t *Belady_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

cache_t *BeladySize_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params);

cache_t *CAR_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

cache_t *Cacheus_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params);

cache_t *Clock_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

cache_t *ClockRI_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params);

cache_t *ClockOracle_init(const common_cache_params_t ccache_params,
                          const char *cache_specific_params);

cache_t *ClockPro_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params);

cache_t *CR_LFU_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

cache_t *FIFO_Merge_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params);

cache_t *GroupMerge_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params);

cache_t *GroupMergeHead_init(const common_cache_params_t ccache_params,
                             const char *cache_specific_params);

cache_t *GroupMergeAdaptive_init(const common_cache_params_t ccache_params,
                                 const char *cache_specific_params);

cache_t *GroupMergeAdaptive2_init(const common_cache_params_t ccache_params,
                                  const char *cache_specific_params);

cache_t *FIFO_Reinsertion_init(const common_cache_params_t ccache_params,
                               const char *cache_specific_params);

cache_t *FIFO_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

cache_t *flashProb_init(const common_cache_params_t ccache_params,
                        const char *cache_specific_params);

cache_t *GDSF_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

cache_t *Hyperbolic_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params);

cache_t *LeCaR_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

cache_t *LeCaRv0_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params);

cache_t *LFU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

cache_t *LFUCpp_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

cache_t *LFUDA_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

cache_t *LHD_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

cache_t *LIRS_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

cache_t *LRU_Prob_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params);

cache_t *LRU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

cache_t *LRUv0_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

cache_t *MRU_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

cache_t *nop_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);

// plugin cache that allows user to implement custom cache
cache_t *pluginCache_init(const common_cache_params_t ccache_params,
                          const char *cache_specific_params);

cache_t *QDLP_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

cache_t *RandomLRU_init(const common_cache_params_t ccache_params,
                        const char *cache_specific_params);

cache_t *RandomTwo_init(const common_cache_params_t ccache_params,
                        const char *cache_specific_params);

cache_t *Random_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

typedef struct {
  cache_t *small_fifo;
  cache_t *ghost_fifo;
  cache_t *main_fifo;
  bool hit_on_ghost;

  int move_to_main_threshold;
  double small_size_ratio;
  double ghost_size_ratio;

  bool has_evicted;
  request_t *req_local;

  int64_t n_obj_promoted;
  int64_t n_byte_promoted;
  int64_t n_obj_rewritten;
  int64_t n_byte_rewritten;
} S3FIFO_params_t;

cache_t *S3FIFO_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

cache_t *S3FIFOd_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params);

cache_t *S3FIFOv0_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params);

cache_t *S3LRU_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

cache_t *SFIFO_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

cache_t *SFIFOv0_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params);

cache_t *Sieve_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);

cache_t *Size_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

cache_t *SLRU_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

cache_t *SLRUv0_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

cache_t *SR_LRU_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params);

cache_t *TwoQ_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params);

cache_t *WTinyLFU_init(const common_cache_params_t ccache_params,
                       const char *cache_specific_params);

#ifdef ENABLE_3L_CACHE
cache_t *ThreeLCache_init(const common_cache_params_t ccache_params,
                          const char *cache_specific_params);
#endif

#ifdef ENABLE_LRB
cache_t *LRB_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params);
#endif

#if defined(ENABLE_GLCACHE)

cache_t *GLCache_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params);

#endif

#ifdef __cplusplus
}
#endif
