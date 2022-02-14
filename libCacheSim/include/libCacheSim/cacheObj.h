//
// Created by Juncheng Yang on 11/17/19.
//

#pragma once

#include "../config.h"
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h> 
#include <stdio.h>
#include "mem.h"

#ifdef __cplusplus
extern "C" {
#endif

// ############## per object metadata used in eviction algorithm cache obj ############
typedef struct {
  bool visited; 
} Clock_obj_params_t; 

typedef struct {
  int64_t last_access_vtime; 
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

typedef struct  {
  int64_t freq;
  int64_t vtime_enter_cache;
  void *pq_node;
} Hyperbolic_obj_metadata_t; 

typedef struct Optimal_obj_metadata {
  void *pq_node;
  int64_t next_access_vtime;
} Optimal_obj_metadata_t;

typedef struct FIFOMerge_obj_metadata {
  int32_t freq; 
  int32_t last_access_vtime; 
  int64_t next_access_vtime; 
} FIFOMerge_obj_metadata_t;

typedef struct L2Cache_obj_metadata {
  void *segment;
  int64_t next_access_vtime;
  int32_t freq;
  int32_t last_access_rtime;
  int16_t idx_in_segment;
  int16_t active : 2;       // whether this object has been acccessed 
  int16_t in_cache : 2;
  int16_t seen_after_snapshot : 2;
//      int16_t n_merged : 12;  /* how many times it has been merged */
} L2Cache_obj_metadata_t;


// ############################## cache obj ###################################
struct cache_obj;
typedef struct cache_obj {
  struct cache_obj *hash_next;
  obj_id_t obj_id;
  uint32_t obj_size;
  struct {
    struct cache_obj *prev;
    struct cache_obj *next;
  } queue; // for LRU, FIFO, etc. 
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  uint32_t exp_time;
#endif
  union {
    struct {
      int64_t freq; 
    } lfu; // for LFU

    Clock_obj_params_t clock; // for Clock
    LeCaR_obj_params_t LeCaR; // for LeCaR
    Cacheus_obj_params_t Cacheus; // for Cacheus
    SR_LRU_obj_params_t SR_LRU;
    CR_LFU_obj_params_t CR_LFU;
    Hyperbolic_obj_metadata_t hyperbolic;
    Optimal_obj_metadata_t optimal; 
    FIFOMerge_obj_metadata_t FIFOMerge;
#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
    L2Cache_obj_metadata_t L2Cache;
#endif
  };
#ifdef TRACK_EVICTION_AGE
#endif
} __attribute__((packed)) cache_obj_t;


struct request;
/**
 * copy the cache_obj to req_dest
 * @param req_dest
 * @param cache_obj
 */
void copy_cache_obj_to_request(struct request *req_dest, cache_obj_t *cache_obj);

/**
 * copy the data from request into cache_obj
 * @param cache_obj
 * @param req
 */
void copy_request_to_cache_obj(cache_obj_t *cache_obj, struct request *req);

/**
 * create a cache_obj from request
 * @param req
 * @return
 */
cache_obj_t *create_cache_obj_from_request(struct request *req); 


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
  while (head != NULL && head->queue.next != cache_obj)
    head = head->queue.next;
  return head;
}

/** remove the object from the LRU queue (a built-in doubly linked list)
 *
 * @param head
 * @param tail
 * @param cache_obj
 */
void remove_obj_from_list(cache_obj_t **head, cache_obj_t **tail, cache_obj_t *cache_obj);

/**
 * move an object to the tail of the LRU queue (a doubly linked list)
 * @param head
 * @param tail
 * @param cache_obj
 */
void move_obj_to_tail(cache_obj_t **head, cache_obj_t **tail, cache_obj_t *cache_obj); 

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

