/**
 * @file OBL.c
 * @brief Implementation of the One-Block Lookahead (OBL) prefetcher.
 *
 * OBL is a simple sequential prefetcher designed for block storage workloads
 * where data is often accessed in a contiguous manner. It works by tracking
 * the last few accessed blocks. If it detects a sequential access pattern
 * of a certain length (the "confidence"), it prefetches the next block
 * in the sequence.
 */

#include "libCacheSim/prefetchAlgo/OBL.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include "libCacheSim/prefetchAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for static functions
static void OBL_handle_find(cache_t *cache, const request_t *req, bool hit);
static void OBL_prefetch(cache_t *cache, const request_t *req);
static void free_OBL_prefetcher(prefetcher_t *prefetcher);
static prefetcher_t *clone_OBL_prefetcher(prefetcher_t *prefetcher, uint64_t cache_size);
static void OBL_parse_init_params(const char *cache_specific_params, OBL_init_params_t *init_params);
static void set_OBL_params(OBL_params_t *OBL_params, OBL_init_params_t *init_params, uint64_t cache_size);

/**
 * @brief Creates an OBL prefetcher instance.
 *
 * @param init_params A string containing initialization parameters.
 * @param cache_size The size of the cache this prefetcher is attached to.
 * @return A pointer to the newly created prefetcher_t structure.
 */
prefetcher_t *create_OBL_prefetcher(const char *init_params, uint64_t cache_size) {
  OBL_init_params_t *obl_init_params = calloc(1, sizeof(OBL_init_params_t));
  set_OBL_default_init_params(obl_init_params);
  if (init_params != NULL) {
    OBL_parse_init_params(init_params, obl_init_params);
  }

  OBL_params_t *obl_params = calloc(1, sizeof(OBL_params_t));
  set_OBL_params(obl_params, obl_init_params, cache_size);

  prefetcher_t *prefetcher = calloc(1, sizeof(prefetcher_t));
  prefetcher->params = obl_params;
  prefetcher->prefetch = OBL_prefetch;
  prefetcher->handle_find = OBL_handle_find;
  prefetcher->handle_insert = NULL;
  prefetcher->handle_evict = NULL;
  prefetcher->free = free_OBL_prefetcher;
  prefetcher->clone = clone_OBL_prefetcher;
  if (init_params) {
    prefetcher->init_params = strdup(init_params);
  }

  free(obl_init_params);
  return prefetcher;
}

/**
 * @brief Frees all resources used by the OBL prefetcher.
 * @param prefetcher The prefetcher to free.
 */
static void free_OBL_prefetcher(prefetcher_t *prefetcher) {
  OBL_params_t *params = (OBL_params_t *)prefetcher->params;
  free(params->prev_access_block);
  free(params);
  if (prefetcher->init_params) {
    free(prefetcher->init_params);
  }
  free(prefetcher);
}

/**
 * @brief Clones an OBL prefetcher instance.
 */
static prefetcher_t *clone_OBL_prefetcher(prefetcher_t *prefetcher, uint64_t cache_size) {
  return create_OBL_prefetcher(prefetcher->init_params, cache_size);
}

/**
 * @brief Handles a cache find event to detect sequential access patterns.
 *
 * This function checks if the current request's object ID continues a
 * sequential pattern based on the last `k` requests stored in `prev_access_block`.
 * If a sequential stream is detected, it sets the `do_prefetch` flag to true.
 *
 * @param cache The cache instance.
 * @param req The request being processed.
 * @param hit Whether the request was a cache hit.
 */
static void OBL_handle_find(cache_t *cache, const request_t *req, bool hit) {
  OBL_params_t *params = (OBL_params_t *)(cache->prefetcher->params);
  int32_t k = params->sequential_confidence_k;

  bool is_sequential = true;
  for (int i = 0; i < k; i++) {
    // Check if the previous k blocks were sequential leading up to the current one
    if (params->prev_access_block[(params->curr_idx + 1 + i) % k] != req->obj_id - k + i) {
      is_sequential = false;
      break;
    }
  }

  params->do_prefetch = is_sequential;
  // Record the current access in the history buffer
  params->curr_idx = (params->curr_idx + 1) % k;
  params->prev_access_block[params->curr_idx] = req->obj_id;
}

/**
 * @brief Issues a prefetch request if a sequential pattern was detected.
 *
 * If the `do_prefetch` flag was set by `OBL_handle_find`, this function
 * will attempt to prefetch the next block in the sequence (`req->obj_id + 1`).
 *
 * @param cache The cache instance.
 * @param req The current request.
 */
static void OBL_prefetch(cache_t *cache, const request_t *req) {
  OBL_params_t *params = (OBL_params_t *)(cache->prefetcher->params);

  if (params->do_prefetch) {
    params->do_prefetch = false; // Reset flag
    request_t *new_req = new_request();
    new_req->obj_size = params->block_size;
    new_req->obj_id = req->obj_id + 1;

    // Don't prefetch if already in cache
    if (cache->find(cache, new_req, false)) {
      free_request(new_req);
      return;
    }

    // Make space and insert
    while (cache->get_occupied_byte(cache) + params->block_size > cache->cache_size) {
      cache->evict(cache, req);
    }
    cache->insert(cache, new_req);
    free_request(new_req);
  }
}

/**
 * @brief Sets the default parameters for the OBL initializer.
 */
static void set_OBL_default_init_params(OBL_init_params_t *init_params) {
  init_params->block_size = 512;
  init_params->sequential_confidence_k = 4;
}

/**
 * @brief Parses algorithm-specific parameters from a string.
 */
static void OBL_parse_init_params(const char *cache_specific_params,
                                  OBL_init_params_t *init_params) {
  char *p_params = strdup(cache_specific_params);
  char *tok = strtok(p_params, ",");
  while (tok != NULL) {
    char *key = strsep(&tok, "=");
    char *value = tok;
    if (strcasecmp(key, "block-size") == 0) {
      init_params->block_size = atoi(value);
    } else if (strcasecmp(key, "sequential-confidence-k") == 0) {
      init_params->sequential_confidence_k = atoi(value);
    } else {
      ERROR("OBL does not have parameter %s\n", key);
    }
    tok = strtok(NULL, ",");
  }
  free(p_params);
}

/**
 * @brief Sets the internal parameters of the OBL prefetcher.
 */
static void set_OBL_params(OBL_params_t *OBL_params,
                           OBL_init_params_t *init_params,
                           uint64_t cache_size) {
  OBL_params->block_size = init_params->block_size;
  OBL_params->sequential_confidence_k = init_params->sequential_confidence_k;
  OBL_params->do_prefetch = false;
  if (OBL_params->sequential_confidence_k <= 0) {
    ERROR("sequential_confidence_k should be positive\n");
    exit(1);
  }
  OBL_params->prev_access_block = calloc(OBL_params->sequential_confidence_k, sizeof(obj_id_t));
  for (int i = 0; i < OBL_params->sequential_confidence_k; i++) {
    OBL_params->prev_access_block[i] = UINT64_MAX;
  }
  OBL_params->curr_idx = 0;
}

#ifdef __cplusplus
}
#endif
