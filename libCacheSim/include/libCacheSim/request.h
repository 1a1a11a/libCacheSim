/**
 * @file request.h
 * @brief Defines the request structure and related functions.
 *
 * This file contains the definition of `request_t`, which represents a single
 * access request from a trace file. It also provides utility functions for
 * creating, copying, and freeing requests.
 */

#ifndef libCacheSim_REQUEST_H
#define libCacheSim_REQUEST_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "enum.h"
#include "logging.h"
#include "mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define N_MAX_FEATURES 16

/**
 * @brief Represents a single cache request.
 *
 * This structure holds all information related to a single access,
 * such as object ID, size, and operation type. It is designed to be
 * mindful of memory layout for performance.
 */
typedef struct request {
  int64_t clock_time;       /**< The timestamp of the request, typically in microseconds. */

  uint64_t hv;              /**< Precomputed hash value of the object ID, can be offloaded to the trace reader. */

  obj_id_t obj_id;          /**< The unique identifier for the object. For block caches, this is the logical block address (LBA). */

  int64_t obj_size;         /**< The size of the object in bytes. */

  int32_t ttl;              /**< The time-to-live for the object. */

  req_op_e op;              /**< The operation type of the request (e.g., GET, SET, DELETE). */

  int32_t tenant_id;        /**< The ID of the tenant making the request. */

  uint64_t n_req;           /**< Request sequence number. */

  int64_t next_access_vtime;/**< The virtual time of the next access to this object (-1 if no next access). */

  /**
   * @brief Fields specific to key-value cache traces.
   */
  struct {
    uint64_t key_size : 16; /**< The size of the key. */
    uint64_t val_size : 48; /**< The size of the value. */
  } kv;

  int32_t ns;               /**< Namespace identifier. */

  void *eviction_algo_data; /**< A generic pointer to carry data for eviction algorithms between function calls. */

  /* Fields primarily used in trace analysis */
  int64_t vtime_since_last_access; /**< Virtual time since the last access to this object. */
  int64_t rtime_since_last_access; /**< Real time since the last access to this object. */
  int64_t prev_size;            /**< The previous size of the object, if it was overwritten. */
  int32_t create_rtime;         /**< The real time when the object was created. */
  bool compulsory_miss;         /**< True if this is the first access to the object. */
  bool overwrite;               /**< True if this request overwrites an existing object. */
  bool first_seen_in_window;    /**< True if this is the first time the object is seen in a time window. */

  bool valid;                   /**< Indicates if the request is valid. Becomes false at the end of a trace. */

  int32_t n_features;           /**< Number of features for ML-based algorithms. */
  int32_t features[N_MAX_FEATURES]; /**< Array of features. */

} request_t;

/**
 * @brief Allocates and initializes a new request_t struct.
 * @return A pointer to the newly allocated request.
 */
static inline request_t *new_request(void) {
  request_t *req = my_malloc(request_t);
  memset(req, 0, sizeof(request_t));
  req->obj_size = 1;
  req->op = OP_NOP;
  req->valid = true;
  req->obj_id = 0;
  req->clock_time = 0;
  req->hv = 0;
  req->next_access_vtime = -2; // -2 indicates not set, -1 indicates no next access
  req->ttl = 0;
  return req;
}

/**
 * @brief Copies the content of one request to another.
 * @param req_dest The destination request.
 * @param req_src The source request.
 */
static inline void copy_request(request_t *req_dest, const request_t *req_src) {
  memcpy(req_dest, req_src, sizeof(request_t));
}

/**
 * @brief Creates a new request that is a duplicate of an existing one.
 * @param req The request to clone.
 * @return A pointer to the newly allocated and copied request.
 */
static inline request_t *clone_request(const request_t *req) {
  request_t *req_new = my_malloc(request_t);
  copy_request(req_new, req);
  return req_new;
}

/**
 * @brief Frees the memory used by a request struct.
 * @param req The request to free.
 */
static inline void free_request(request_t *req) { my_free(request_t, req); }

/**
 * @brief Prints the details of a request for debugging purposes.
 * @param req The request to print.
 */
static inline void print_request(const request_t *req) {
#ifdef SUPPORT_TTL
  LOGGING(DEBUG_LEVEL,
          "req clock_time %lu, id %llu, size %ld, ttl %ld, op %s, valid %d\n",
          (unsigned long)req->clock_time, (unsigned long long)req->obj_id,
          (long)req->obj_size, (long)req->ttl, req_op_str[req->op], req->valid);
#else
  LOGGING(DEBUG_LEVEL,
          "req clock_time %lu, id %llu, size %ld, op %s, valid %d\n",
          (unsigned long)req->clock_time, (unsigned long long)req->obj_id,
          (long)req->obj_size, req_op_str[req->op], req->valid);
#endif
}

#ifdef __cplusplus
}
#endif

#endif  // libCacheSim_REQUEST_H
