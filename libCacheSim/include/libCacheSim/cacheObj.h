//
// Created by Juncheng Yang on 11/17/19.
//

#pragma once

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "../config.h"
#include "mem.h"

#ifdef __cplusplus
extern "C" {
#endif

// ############## per object metadata used in eviction algorithm cache obj
typedef struct {
  int64_t freq;
} LFU_obj_metadata_t;

typedef struct {
  int freq;
} Clock_obj_metadata_t;

typedef enum {
  CLOCKPRO_TEST,
  CLOCKPRO_COLD,
  CLOCKPRO_HOT
} clockpro_status_e;

typedef struct {
  clockpro_status_e status;
  bool referenced;
} ClockPro_obj_metadata_t;

typedef struct {
  void *pq_node;
} Size_obj_metadata_t;

typedef struct {
  int lru_id;
  bool ghost;
} ARC_obj_metadata_t;

typedef struct {
  void *lfu_next;
  void *lfu_prev;
  int64_t eviction_vtime:40;
  int64_t freq:23;
  int64_t is_ghost:1;
  int8_t evict_expert; // 1: LRU, 2: LFU
} __attribute__((packed)) LeCaR_obj_metadata_t;

typedef struct {
  int64_t last_access_vtime;
} Cacheus_obj_metadata_t;

typedef struct {
  bool demoted;
  bool new_obj;
} SR_LRU_obj_metadata_t;

typedef struct {
  int64_t last_access_vtime;
  int64_t freq;
} CR_LFU_obj_metadata_t;

typedef struct {
  int64_t vtime_enter_cache:40;
  int64_t freq:24;
  void *pq_node;
} Hyperbolic_obj_metadata_t;

typedef struct Belady_obj_metadata {
  void *pq_node;
  int64_t next_access_vtime;
} Belady_obj_metadata_t;

typedef struct {
  bool is_LIR;
  bool in_cache;
} LIRS_obj_metadata_t;

typedef struct FIFOMerge_obj_metadata {
  int32_t freq;
  int32_t last_access_vtime;
} FIFO_Merge_obj_metadata_t;

typedef struct {
  int32_t freq;
  int32_t last_access_vtime;
} FIFO_Reinsertion_obj_metadata_t;

typedef struct {
  void *segment;
  int32_t freq;
  int32_t last_access_rtime;
  int32_t last_access_vtime;
  int16_t idx_in_segment;
  int16_t active : 2;  // whether this object has been accessed
  int16_t in_cache : 2;
  int16_t seen_after_snapshot : 2;
} GLCache_obj_metadata_t;

typedef struct {
  int lru_id;
} SLRU_obj_metadata_t;

typedef struct {
  int64_t last_access_vtime;
  int64_t insertion_time;
  int32_t oracle_idx;
} Random_obj_metadata_t;

typedef struct {
  int64_t last_access_vtime;
  int32_t freq;
  int8_t fifo_id;
} SFIFO_obj_metadata_t;

typedef struct {
  int32_t freq;
  int32_t last_access_time;
  int32_t cache_id;  // 1: fifo, 2: clock, 3: fifo_ghost
  bool visited;
} QDLP_obj_metadata_t;

typedef struct {
  int64_t insertion_time;   // measured in number of objects inserted
  int64_t freq;
  int32_t main_insert_freq;
} S3FIFO_obj_metadata_t;

typedef struct {
  int32_t freq;
} __attribute__((packed)) Sieve_obj_params_t;

typedef struct {
  int64_t next_access_vtime;
  int32_t freq;
} __attribute__((packed)) misc_metadata_t;

// ############################## cache obj ###################################
struct cache_obj;
typedef struct cache_obj {
  struct cache_obj *hash_next;
  obj_id_t obj_id;
  uint64_t obj_size;
  struct {
    struct cache_obj *prev;
    struct cache_obj *next;
  } queue;  // for LRU, FIFO, etc.
#ifdef SUPPORT_TTL
  uint32_t exp_time;
#endif
/* age is defined as the time since the object entered the cache */
#if defined(TRACK_EVICTION_V_AGE) || \
    defined(TRACK_DEMOTION) || defined(TRACK_CREATE_TIME)
  int64_t create_time;
#endif
  // used by belady related algorithms
  misc_metadata_t misc;

  union {
    LFU_obj_metadata_t lfu;          // for LFU
    Clock_obj_metadata_t clock;      // for Clock
    ClockPro_obj_metadata_t clockpro;// for ClockPro
    Size_obj_metadata_t Size;        // for Size
    ARC_obj_metadata_t ARC;          // for ARC
    LeCaR_obj_metadata_t LeCaR;      // for LeCaR
    Cacheus_obj_metadata_t Cacheus;  // for Cacheus
    SR_LRU_obj_metadata_t SR_LRU;
    CR_LFU_obj_metadata_t CR_LFU;
    Hyperbolic_obj_metadata_t hyperbolic;
    Random_obj_metadata_t Random;
    Belady_obj_metadata_t Belady;
    FIFO_Merge_obj_metadata_t FIFO_Merge;
    FIFO_Reinsertion_obj_metadata_t FIFO_Reinsertion;
    SFIFO_obj_metadata_t SFIFO;
    SLRU_obj_metadata_t SLRU;
    QDLP_obj_metadata_t QDLP;
    LIRS_obj_metadata_t LIRS;
    S3FIFO_obj_metadata_t S3FIFO;
    Sieve_obj_params_t sieve;

#if defined(ENABLE_GLCACHE) && ENABLE_GLCACHE == 1
    GLCache_obj_metadata_t GLCache;
#endif
  };
} __attribute__((packed)) cache_obj_t;

struct request;
/**
 * copy the cache_obj to req_dest
 * @param req_dest
 * @param cache_obj
 */
void copy_cache_obj_to_request(struct request *req_dest,
                               const cache_obj_t *cache_obj);

/**
 * copy the data from request into cache_obj
 * @param cache_obj
 * @param req
 */
void copy_request_to_cache_obj(cache_obj_t *cache_obj,
                               const struct request *req);

/**
 * create a cache_obj from request
 * @param req
 * @return
 */
cache_obj_t *create_cache_obj_from_request(const struct request *req);

/**
 * the cache_obj has built-in a doubly list, in the case the list is used as
 * a singly list (list_prev is not used, next is used)
 * so this function finds the list_prev element in the list
 *
 * NOTE: this is an expensive op
 * @param head
 * @param cache_obj
 * @return
 */
static inline cache_obj_t *prev_obj_in_slist(cache_obj_t *head,
                                             cache_obj_t *cache_obj) {
  assert(head != cache_obj);
  while (head != NULL && head->queue.next != cache_obj) head = head->queue.next;
  return head;
}

/** remove the object from the LRU queue (a built-in doubly linked list)
 *
 * @param head
 * @param tail
 * @param cache_obj
 */
void remove_obj_from_list(cache_obj_t **head, cache_obj_t **tail,
                          cache_obj_t *cache_obj);

/**
 * move an object to the tail of the LRU queue (a doubly linked list)
 * @param head
 * @param tail
 * @param cache_obj
 */
void move_obj_to_tail(cache_obj_t **head, cache_obj_t **tail,
                      cache_obj_t *cache_obj);

/**
 * move an object to the head of the LRU queue (a doubly linked list)
 * @param head
 * @param tail
 * @param cache_obj
 */
void move_obj_to_head(cache_obj_t **head, cache_obj_t **tail,
                      cache_obj_t *cache_obj);

/**
 * prepend the object to the head of the doubly linked list
 * the object should not be in the list, otherwise, use move_obj_to_head
 * @param head
 * @param tail
 * @param cache_obj
 */
void prepend_obj_to_head(cache_obj_t **head, cache_obj_t **tail,
                         cache_obj_t *cache_obj);

/**
 * append the object to the tail of the doubly linked list
 * the object should not be in the list, otherwise, use move_obj_to_tail
 * @param head
 * @param tail
 * @param cache_obj
 */
void append_obj_to_tail(cache_obj_t **head, cache_obj_t **tail,
                        cache_obj_t *cache_obj);
/**
 * free cache_obj, this is only used when the cache_obj is explicitly
 * malloced
 * @param cache_obj
 */
static inline void free_cache_obj(cache_obj_t *cache_obj) {
  my_free(sizeof(cache_obj_t), cache_obj);
}

#ifdef __cplusplus
}
#endif
