//
// Created by Juncheng Yang on 5/14/20.
//

#ifndef libCacheSim_STRUCT_H
#define libCacheSim_STRUCT_H

#include "enum.h"
#include "../config.h"    // obj_id_t 
#include <stdbool.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

// // ############## per object metadata used in eviction algorithm cache obj ############
// typedef struct {
//   bool visited; 
// } Clock_obj_params_t; 

// typedef struct {
//   int64_t last_access_vtime; 
// } LeCaR_obj_params_t;

// typedef struct  {
//   int64_t freq;
//   int64_t vtime_enter_cache;
//   void *pq_node;
// } Hyperbolic_obj_metadata_t; 

// typedef struct Optimal_obj_metadata {
//   void *pq_node;
//   int64_t next_access_vtime;
// } Optimal_obj_metadata_t;

// typedef struct L2Cache_obj_metadata {
//   void *segment;
//   int64_t next_access_vtime;
//   int32_t freq;
//   int32_t last_access_rtime;
//   int16_t idx_in_segment;
//   int16_t active : 2;
//   int16_t in_cache : 2;
//   int16_t seen_after_snapshot : 2;
// //      int16_t n_merged : 12;  /* how many times it has been merged */
// } L2Cache_obj_metadata_t;


// // ############################## cache obj ###################################
// struct cache_obj;
// typedef struct cache_obj {
//   struct cache_obj *hash_next;
//   obj_id_t obj_id;
//   uint32_t obj_size;
//   struct {
//     struct cache_obj *prev;
//     struct cache_obj *next;
//   } queue; // for LRU, FIFO, etc. 
// #if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
//   uint32_t exp_time;
// #endif
//   union {
//     struct {
//       int64_t freq; 
//     } lfu; // for LFU

//     Clock_obj_params_t clock; // for Clock
//     LeCaR_obj_params_t LeCaR; // for LeCaR
//     Hyperbolic_obj_metadata_t hyperbolic;
//     Optimal_obj_metadata_t optimal; 
// #if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
//     L2Cache_obj_metadata_t L2Cache;
// #endif
//   };
// #ifdef TRACK_EVICTION_AGE
// #endif
// } __attribute__((packed)) cache_obj_t;


#ifdef __cplusplus
}
#endif

#endif// libCacheSim_STRUCT_H
