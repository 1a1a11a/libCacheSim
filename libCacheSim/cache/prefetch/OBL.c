//
//  an OBL module that supports sequential prefetching for block storage. Each
//  object (logical block address) should be uniform in size.
//
//
//  OBL.c
//  libCacheSim
//
//  Created by Zhelong on 24/1/29.
//  Copyright © 2024 Zhelong. All rights reserved.
//
#include "../../include/libCacheSim/prefetchAlgo/OBL.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>

#include "../../include/libCacheSim/prefetchAlgo.h"
// #define DEBUG

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
// ****                                                               ****
// ****               helper function declarations                    ****
// ****                                                               ****
// ***********************************************************************

const char *OBL_default_params(void) { return "block-size=512, sequential-confidence-k=4"; }

static void set_OBL_default_init_params(OBL_init_params_t *init_params) {
  init_params->block_size = 512;
  init_params->sequential_confidence_k = 4;
}

static void OBL_parse_init_params(const char *cache_specific_params, OBL_init_params_t *init_params) {
  char *params_str = strdup(cache_specific_params);

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }
    if (strcasecmp(key, "block-size") == 0) {
      init_params->block_size = atoi(value);
    } else if (strcasecmp(key, "sequential-confidence-k") == 0) {
      init_params->sequential_confidence_k = atoi(value);
    } else {
      ERROR("OBL does not have parameter %s\n", key);
      printf("default params: %s\n", OBL_default_params());
      exit(1);
    }
  }
}

static void set_OBL_params(OBL_params_t *OBL_params, OBL_init_params_t *init_params, uint64_t cache_size) {
  OBL_params->block_size = init_params->block_size;
  OBL_params->sequential_confidence_k = init_params->sequential_confidence_k;
  OBL_params->do_prefetch = false;
  if (OBL_params->sequential_confidence_k <= 0) {
    printf("sequential_confidence_k should be positive\n");
    exit(1);
  }
  OBL_params->prev_access_block = (obj_id_t *)malloc(OBL_params->sequential_confidence_k * sizeof(obj_id_t));
  for (int i = 0; i < OBL_params->sequential_confidence_k; i++) {
    OBL_params->prev_access_block[i] = UINT64_MAX;
  }
  OBL_params->curr_idx = 0;
}

/**************************************************************************
 **                      prefetcher interfaces
 **
 ** create, free, clone, handle_find, handle_insert, handle_evict, prefetch
 **************************************************************************/
prefetcher_t *create_OBL_prefetcher(const char *init_params, uint64_t cache_size);
/**
 check if the previous access is sequential. If true, set do_prefetch to true.

@param cache the cache struct
@param req the request containing the request
@return
*/
static void OBL_handle_find(cache_t *cache, const request_t *req, bool hit) {
  OBL_params_t *OBL_params = (OBL_params_t *)(cache->prefetcher->params);
  int32_t sequential_confidence_k = OBL_params->sequential_confidence_k;

  // assert(req->obj_size == OBL_params->block_size);
  bool flag = true;
  for (int i = 0; i < sequential_confidence_k; i++) {
    if (OBL_params->prev_access_block[(OBL_params->curr_idx + 1 + i) % sequential_confidence_k] !=
        req->obj_id - sequential_confidence_k + i) {
      flag = false;
      break;
    }
  }
  OBL_params->do_prefetch = flag;
  OBL_params->curr_idx = (OBL_params->curr_idx + 1) % sequential_confidence_k;
  OBL_params->prev_access_block[OBL_params->curr_idx] = req->obj_id;
}

/**
 prefetch next block if the previous access is sequential

 @param cache the cache struct
 @param req the request containing the request
 @return
 */
void OBL_prefetch(cache_t *cache, const request_t *req) {
  OBL_params_t *OBL_params = (OBL_params_t *)(cache->prefetcher->params);

  if (OBL_params->do_prefetch) {
    OBL_params->do_prefetch = false;
    request_t *new_req = new_request();
    new_req->obj_size = OBL_params->block_size;
    new_req->obj_id = req->obj_id + 1;
    if (cache->find(cache, new_req, false)) {
      free_request(new_req);
      return;
    }
    while (cache->get_occupied_byte(cache) + OBL_params->block_size > cache->cache_size) {
      cache->evict(cache, req);
    }
    cache->insert(cache, new_req);
    free_request(new_req);
  }
}

void free_OBL_prefetcher(prefetcher_t *prefetcher) {
  OBL_params_t *OBL_params = (OBL_params_t *)prefetcher->params;
  free(OBL_params->prev_access_block);

  my_free(sizeof(OBL_params_t), OBL_params);
  if (prefetcher->init_params) {
    free(prefetcher->init_params);
  }
  my_free(sizeof(prefetcher_t), prefetcher);
}

prefetcher_t *clone_OBL_prefetcher(prefetcher_t *prefetcher, uint64_t cache_size) {
  return create_OBL_prefetcher(prefetcher->init_params, cache_size);
}

prefetcher_t *create_OBL_prefetcher(const char *init_params, uint64_t cache_size) {
  OBL_init_params_t *OBL_init_params = my_malloc(OBL_init_params_t);
  memset(OBL_init_params, 0, sizeof(OBL_init_params_t));

  set_OBL_default_init_params(OBL_init_params);
  if (init_params != NULL) {
    OBL_parse_init_params(init_params, OBL_init_params);
  }

  OBL_params_t *OBL_params = my_malloc(OBL_params_t);
  set_OBL_params(OBL_params, OBL_init_params, cache_size);

  prefetcher_t *prefetcher = (prefetcher_t *)my_malloc(prefetcher_t);
  memset(prefetcher, 0, sizeof(prefetcher_t));
  prefetcher->params = OBL_params;
  prefetcher->prefetch = OBL_prefetch;
  prefetcher->handle_find = OBL_handle_find;
  prefetcher->handle_insert = NULL;
  prefetcher->handle_evict = NULL;
  prefetcher->free = free_OBL_prefetcher;
  prefetcher->clone = clone_OBL_prefetcher;
  if (init_params) {
    prefetcher->init_params = strdup(init_params);
  }

  my_free(sizeof(OBL_init_params_t), OBL_init_params);
  return prefetcher;
}