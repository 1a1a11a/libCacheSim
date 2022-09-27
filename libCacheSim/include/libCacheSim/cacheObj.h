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
  bool visited;
} Clock_obj_params_t;

typedef struct {
  int lru_id;
  bool ghost;
} ARC_obj_params_t;

typedef struct {
  int64_t eviction_vtime;
  int64_t next_access_vtime;
  void *lfu_next;
  void *lfu_prev;
  int32_t freq;
  bool ghost_evicted_by_lru;
  bool ghost_evicted_by_lfu;
} LeCaR_obj_params_t;

typedef struct {
  int64_t last_access_vtime;
} Cacheus_obj_params_t;

typedef struct {
  bool demoted;
  bool new_obj;
} SR_LRU_obj_params_t;

typedef struct {
  int64_t last_access_vtime;
  int64_t freq;
} CR_LFU_obj_params_t;

typedef struct {
  int64_t freq;
  int64_t vtime_enter_cache;
  void *pq_node;
} Hyperbolic_obj_metadata_t;

typedef struct Belady_obj_metadata {
  void *pq_node;
  int64_t next_access_vtime;
} Belady_obj_metadata_t;

typedef struct FIFOMerge_obj_metadata {
  int32_t freq;
  int32_t last_access_vtime;
  int64_t next_access_vtime;
} FIFOMerge_obj_metadata_t;

typedef struct FIFO_readmission_obj_metadata {
  int32_t freq;
  int32_t last_access_vtime;
  int64_t next_access_vtime;
} FIFO_readmission_obj_metadata_t;

typedef struct {
  void *segment;
  int64_t next_access_vtime;
  int32_t freq;
  int32_t last_access_rtime;
  int32_t last_access_vtime;
  int16_t idx_in_segment;
  int16_t active : 2;  // whether this object has been acccessed
  int16_t in_cache : 2;
  int16_t seen_after_snapshot : 2;
  //      int16_t n_merged : 12;  /* how many times it has been merged */
} GLCache_obj_metadata_t;

#define DEBUG_MODE
// ############################## cache obj ###################################
struct cache_obj;
typedef struct cache_obj {
#ifdef DEBUG_MODE
  int32_t magic;
#endif
  struct cache_obj *hash_next;
  obj_id_t obj_id;
  uint32_t obj_size;
  struct {
    struct cache_obj *prev;
    struct cache_obj *next;
  } queue;  // for LRU, FIFO, etc.
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  uint32_t exp_time;
#endif
  union {
    struct {
      int64_t freq;
    } lfu;  // for LFU

    Clock_obj_params_t clock;      // for Clock
    ARC_obj_params_t ARC2;         // for ARC
    LeCaR_obj_params_t LeCaR;      // for LeCaR
    Cacheus_obj_params_t Cacheus;  // for Cacheus
    SR_LRU_obj_params_t SR_LRU;
    CR_LFU_obj_params_t CR_LFU;
    Hyperbolic_obj_metadata_t hyperbolic;
    Belady_obj_metadata_t Belady;
    FIFOMerge_obj_metadata_t FIFOMerge;
    FIFO_readmission_obj_metadata_t FIFO_readmission;
#if defined(ENABLE_GLCache) && ENABLE_GLCache == 1
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
 * free cache_obj, this is only used when the cache_obj is explicitly malloced
 * @param cache_obj
 */
static inline void free_cache_obj(cache_obj_t *cache_obj) {
  my_free(sizeof(cache_obj_t), cache_obj);
}

#ifdef __cplusplus
}
#endif
