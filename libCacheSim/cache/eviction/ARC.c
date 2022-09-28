//
//  ARC cache replacement algorithm
//  libCacheSim
//
//  Created by Juncheng on 09/28/20.
//  Copyright © 2020 Juncheng. All rights reserved.
//

#include "../include/libCacheSim/evictionAlgo/ARC.h"

#include <string.h>

#include "../dataStructure/hashtable/hashtable.h"
#include "../include/libCacheSim/evictionAlgo/LRU.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ARC_params {
  cache_t *LRU1;             // LRU
  cache_t *LRU1g;            // ghost LRU
  cache_t *LRU2;             // LRU for items accessed more than once
  cache_t *LRU2g;            // ghost LRU for items accessed more than once3
  double ghost_list_factor;  // size(ghost_list)/size(cache), default 1
  int evict_lru;             // which LRU list the eviction should come from
} ARC_params_t;

// typedef struct ARC_init_params {
//   double ghost_list_factor;
// } ARC_init_params_t;

const char *ARC_default_init_params(void) { return "ghost_list_factor=1.0;"; }

cache_t *ARC_init(const common_cache_params_t ccache_params,
                  const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("ARC", ccache_params);
  cache->cache_init = ARC_init;
  cache->cache_free = ARC_free;
  cache->get = ARC_get;
  cache->check = ARC_check;
  cache->insert = ARC_insert;
  cache->evict = ARC_evict;
  cache->remove = ARC_remove;
  cache->to_evict = ARC_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    // two pointer + one history
    cache->per_obj_metadata_size = 8 * 3;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  cache->eviction_params = my_malloc_n(ARC_params_t, 1);
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  params->ghost_list_factor = 1;

  if (cache_specific_params != NULL) {
    char *params_str = strdup(cache_specific_params);

    while (params_str != NULL && params_str[0] != '\0') {
      char *key = strsep((char **)&params_str, "=");
      char *value = strsep((char **)&params_str, ";");
      while (params_str != NULL && *params_str == ' ') {
        params_str++;
      }
      if (strcasecmp(key, "ghost_list_factor") == 0) {
        params->ghost_list_factor = atof(value);
      } else {
        ERROR("%s does not have parameter %s\n", cache->cache_name, key);
        exit(1);
      }
    }

    free(params_str);
  }

  /* the two LRU are initialized with cache_size, but they will not be full */
  params->LRU1 = LRU_init(ccache_params, NULL);
  params->LRU2 = LRU_init(ccache_params, NULL);

  common_cache_params_t ccache_params_ghost = ccache_params;
  ccache_params_ghost.cache_size = (uint64_t)((double)ccache_params.cache_size /
                                              2 * params->ghost_list_factor);
  params->LRU1g = LRU_init(ccache_params_ghost, NULL);
  params->LRU2g = LRU_init(ccache_params_ghost, NULL);

  return cache;
}

void ARC_free(cache_t *cache) {
  ARC_params_t *ARC_params = (ARC_params_t *)(cache->eviction_params);
  ARC_params->LRU1->cache_free(ARC_params->LRU1);
  ARC_params->LRU1g->cache_free(ARC_params->LRU1g);
  ARC_params->LRU2->cache_free(ARC_params->LRU2);
  ARC_params->LRU2g->cache_free(ARC_params->LRU2g);
  my_free(sizeof(ARC_params_t), ARC_params);
  cache_struct_free(cache);
}

void _verify(cache_t *cache, const request_t *req) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_ck_res_e hit1 = params->LRU1->check(params->LRU1, req, false);
  cache_ck_res_e hit2 = params->LRU2->check(params->LRU2, req, false);

  if (hit1 == cache_ck_hit) DEBUG_ASSERT(hit2 != cache_ck_hit);

  if (hit2 == cache_ck_hit) DEBUG_ASSERT(hit1 != cache_ck_hit);

  //  printf("%llu obj %llu %s %s\n", req->n_req, req->obj_id,
  //  CACHE_CK_STATUS_STR[hit1], CACHE_CK_STATUS_STR[hit2]);
}

cache_ck_res_e ARC_check(cache_t *cache, const request_t *req,
                         const bool update_cache) {
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) req_local = new_request();

  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  cache_ck_res_e hit1, hit2, hit1g = cache_ck_invalid, hit2g = cache_ck_invalid;
  /* LRU1 does not update because if it is a hit, we will move it to LRU2 */
  hit1 = params->LRU1->check(params->LRU1, req, false);
  hit2 = params->LRU2->check(params->LRU2, req, update_cache);
  cache_ck_res_e hit = hit1;
  if (hit1 == cache_ck_hit || hit2 == cache_ck_hit) {
    hit = cache_ck_hit;
  }

  if (!update_cache) return hit;

  if (hit != cache_ck_hit) {
    hit1g = params->LRU1g->check(params->LRU1g, req, false);
    hit2g = params->LRU2g->check(params->LRU2g, req, false);
    if (hit1g == cache_ck_hit) {
      /* hit on LRU1 ghost list */
      params->evict_lru = 2;
      DEBUG_ASSERT(hit2g != cache_ck_hit);
      params->LRU1g->remove(params->LRU1g, req->obj_id);
    } else {
      if (hit2g == cache_ck_hit) {
        params->evict_lru = 1;
        params->LRU2g->remove(params->LRU2g, req->obj_id);
      }
    }
  }

  if (hit1 == cache_ck_hit) {
    DEBUG_ASSERT(hit2 != cache_ck_hit);
    params->LRU1->remove(params->LRU1, req->obj_id);
    params->LRU2->insert(params->LRU2, req);
  } else if (hit2 == cache_ck_hit) {
    /* moving to the head of LRU2 has already been done */
  }
  cache->occupied_size =
      params->LRU1->occupied_size + params->LRU2->occupied_size;
  DEBUG_ASSERT(cache->n_obj == params->LRU1->n_obj + params->LRU2->n_obj);

  return hit;
}

cache_ck_res_e ARC_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

void ARC_insert(cache_t *cache, const request_t *req) {
  /* first time add, then it should be add to LRU1 */
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);

  params->LRU1->insert(params->LRU1, req);

  cache->occupied_size += req->obj_size + cache->per_obj_metadata_size;
  cache->n_obj += 1;
  DEBUG_ASSERT(cache->occupied_size ==
               params->LRU1->occupied_size + params->LRU2->occupied_size);
}

cache_obj_t *ARC_to_evict(cache_t *cache) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_t *cache_evict;

  if ((params->evict_lru == 1 || params->LRU2->n_obj == 0) &&
      params->LRU1->n_obj != 0) {
    cache_evict = params->LRU1;
  } else {
    cache_evict = params->LRU2;
  }
  return cache_evict->to_evict(cache_evict);
}

void ARC_evict(cache_t *cache, const request_t *req, cache_obj_t *evicted_obj) {
  cache_obj_t obj;
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_t *cache_evict, *cache_evict_ghost;

  if ((params->evict_lru == 1 || params->LRU2->n_obj == 0) &&
      params->LRU1->n_obj != 0) {
    cache_evict = params->LRU1;
    cache_evict_ghost = params->LRU1g;
  } else {
    cache_evict = params->LRU2;
    cache_evict_ghost = params->LRU2g;
  }

  cache_evict->evict(cache_evict, req, &obj);
  if (evicted_obj != NULL) {
    memcpy(evicted_obj, &obj, sizeof(cache_obj_t));
  }
  copy_cache_obj_to_request(req_local, &obj);
  cache_ck_res_e ck = cache_evict_ghost->get(cache_evict_ghost, req_local);
  DEBUG_ASSERT(ck == cache_ck_miss);
  cache->occupied_size -= (obj.obj_size + cache->per_obj_metadata_size);
  cache->n_obj -= 1;
}

void ARC_remove(cache_t *cache, const obj_id_t obj_id) {
  ARC_params_t *params = (ARC_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_get_obj_by_id(params->LRU1, obj_id);
  if (obj != NULL) {
    params->LRU1->remove(params->LRU1, obj_id);
  } else {
    obj = cache_get_obj_by_id(params->LRU2, obj_id);
    if (obj != NULL) {
      params->LRU2->remove(params->LRU2, obj_id);
    } else {
      PRINT_ONCE("remove object %" PRIu64 "that is not cached\n", obj_id);
      return;
    }
  }

  cache->occupied_size -= (obj->obj_size + cache->per_obj_metadata_size);
  cache->n_obj -= 1;
}

#ifdef __cplusplus
}
#endif
