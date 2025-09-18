/**
 * @file evictionAlgo.h
 * @brief Declares the initialization functions for all available eviction algorithms.
 *
 * Each eviction algorithm is implemented as a separate module and exposes an `_init`
 * function. This function creates and returns a `cache_t` structure with its
 * function pointers configured for that specific algorithm's logic.
 */

#pragma once

#include "cache.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parameters for FIFO-based eviction algorithms.
 */
typedef struct {
  cache_obj_t *q_head; /**< The head of the FIFO queue. */
  cache_obj_t *q_tail; /**< The tail of the FIFO queue. */
} FIFO_params_t;

/**
 * @brief Parameters for LRU-based eviction algorithms.
 */
typedef struct {
  cache_obj_t *q_head; /**< The head of the LRU list (most recently used). */
  cache_obj_t *q_tail; /**< The tail of the LRU list (least recently used). */
} LRU_params_t;

/**
 * @brief A node in a frequency list, used by LFU and related algorithms.
 */
typedef struct freq_node {
  int64_t freq;           /**< The frequency count for this node. */
  cache_obj_t *first_obj; /**< The first object in the doubly linked list of objects with this frequency. */
  cache_obj_t *last_obj;  /**< The last object in the doubly linked list. */
  int32_t n_obj;          /**< The number of objects with this frequency. */
} freq_node_t;

/**
 * @brief Parameters for Clock-based eviction algorithms.
 */
typedef struct {
  cache_obj_t *q_head;      /**< The head of the circular list (clock hand). */
  cache_obj_t *q_tail;      /**< The tail of the circular list. */
  int32_t n_bit_counter;    /**< The number of bits used for the reference counter. */
  int32_t max_freq;         /**< The maximum frequency value (2^(n_bit_counter - 1)). */
  int32_t init_freq;        /**< The initial frequency for new objects. */
  int64_t n_obj_rewritten;  /**< Statistics: number of objects rewritten. */
  int64_t n_byte_rewritten; /**< Statistics: number of bytes rewritten. */
} Clock_params_t;


// The following are initialization functions for various cache eviction algorithms.
// Each function takes common cache parameters and an optional algorithm-specific
// parameter string, and returns a fully initialized cache_t structure.

cache_t *ARC_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *ARCv0_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *Belady_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *BeladySize_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *CAR_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *Cacheus_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *Clock_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *ClockPro_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *CR_LFU_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *FIFO_Merge_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *FIFO_Reinsertion_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *FIFO_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *flashProb_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *GDSF_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *Hyperbolic_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *LeCaR_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *LeCaRv0_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *LFU_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *LFUCpp_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *LFUDA_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *LHD_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *LIRS_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *LRU_Prob_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *LRU_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *LRUv0_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *MRU_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *nop_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *pluginCache_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *QDLP_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *RandomLRU_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *RandomTwo_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *Random_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *S3FIFO_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *S3FIFOd_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *S3FIFOv0_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *S3LRU_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *SFIFO_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *SFIFOv0_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *Sieve_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *Size_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *SLRU_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *SLRUv0_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *SR_LRU_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *TwoQ_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
cache_t *WTinyLFU_init(const common_cache_params_t ccache_params, const char *cache_specific_params);

#ifdef ENABLE_3L_CACHE
cache_t *ThreeLCache_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
#endif

#ifdef ENABLE_LRB
cache_t *LRB_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
#endif

#if defined(ENABLE_GLCACHE)
cache_t *GLCache_init(const common_cache_params_t ccache_params, const char *cache_specific_params);
#endif

#ifdef __cplusplus
}
#endif
