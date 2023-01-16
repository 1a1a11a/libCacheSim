//
//  a novel approximate LFU structure (TinyLFU) admission control
//  S2LRU as eviction policy
//  combined with a windowed LRU structure (W-TinyLFU)
//  to resist burst traffic of temporal hot objects
//
//  NOTE: it is NOT thread-safe
//
//  part of the details borrowed from https://github.com/mandreyel/w-tinylfu
//
//  WTinyLFUv0.c
//  libCacheSim
//
//  Created by Ziyue on 7/1/2023.
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo.h"

#include "../../dataStructure/minimalIncrementCBF.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_MODE
#undef DEBUG_MODE

typedef struct WTinyLFUv0_params {
  cache_t *LRU;  // LRU as windowed LRU
  cache_t *SLRU; // S2LRU as eviction policy
  double window_size;
  struct minimalIncrementCBF * CBF;
  size_t max_request_num;
  size_t request_counter;
} WTinyLFUv0_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void WTinyLFUv0_free(cache_t *cache);
static cache_obj_t * WTinyLFUv0_find(cache_t *cache, const request_t *req, const bool update);
static bool WTinyLFUv0_get(cache_t *cache, const request_t *req);
static cache_obj_t *WTinyLFUv0_insert(cache_t *cache, const request_t *req);
static cache_obj_t *WTinyLFUv0_to_evict(cache_t *cache, const request_t *req);
static void WTinyLFUv0_evict(cache_t *cache, const request_t *req);
static bool WTinyLFUv0_remove(cache_t *cache, const obj_id_t obj_id);



static void WTinyLFUv0_parse_params(cache_t *cache,
                                    const char *cache_specific_params) {
  WTinyLFUv0_params_t *params = (WTinyLFUv0_params_t *)cache->eviction_params;

  // default parameters (TODO: make it configurable)
#ifdef DEBUG_MODE
  params->window_size = 0.01;
#else
  params->window_size = 0.01;
#endif

  params->max_request_num = 1000000; // 1 million, TODO @ Ziyue

  return;
}

/* WTinyLFUv0 cannot an object larger than segment size */
bool WTinyLFUv0_can_insert(cache_t *cache, const request_t *req) {
  WTinyLFUv0_params_t *params = (WTinyLFUv0_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);

  #ifdef DEBUG_MODE
  if(can_insert &&
      (req->obj_size + cache->obj_md_size <= params->LRU->cache_size) &&
      (params->SLRU->can_insert(params->SLRU, req)) == false) {
    printf("[can_insert FALSE] req object size: %lu, cache object md size: %u, cache size: %lu, SLRU can insert: %d\n",
      req->obj_size, cache->obj_md_size, params->LRU->cache_size, params->SLRU->can_insert(params->SLRU, req));
    assert(false);
  }
  #endif

  return can_insert &&
         (req->obj_size + cache->obj_md_size <= params->LRU->cache_size) &&
          (params->SLRU->can_insert(params->SLRU, req));
}

int64_t WTinyLFUv0_get_occupied_byte(const cache_t *cache) {
  WTinyLFUv0_params_t *params = (WTinyLFUv0_params_t *)cache->eviction_params;
  int64_t occupied_byte = 0;
  occupied_byte += params->LRU->occupied_byte;
  occupied_byte += params->SLRU->get_occupied_byte(params->SLRU);

#ifdef DEBUG_MODE
  printf("[get_occupied_byte] LRU: %ld, SLRU: %ld\n\n",
    params->LRU->occupied_byte, params->SLRU->get_occupied_byte(params->SLRU));
#endif

  return occupied_byte;
}

int64_t WTinyLFUv0_get_n_obj(const cache_t *cache) {
  WTinyLFUv0_params_t *params = (WTinyLFUv0_params_t *)cache->eviction_params;
  int64_t n_obj = 0;
  n_obj += params->LRU->n_obj;
  n_obj += params->SLRU->n_obj;
  return n_obj;
}

cache_t *WTinyLFUv0_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("WTinyLFUv0", ccache_params);
  cache->cache_init = WTinyLFUv0_init;
  cache->cache_free = WTinyLFUv0_free;
  cache->get = WTinyLFUv0_get;
  cache->find = WTinyLFUv0_find;
  cache->insert = WTinyLFUv0_insert;
  cache->evict = WTinyLFUv0_evict;
  cache->remove = WTinyLFUv0_remove;
  cache->to_evict = WTinyLFUv0_to_evict;
  cache->init_params = cache_specific_params;
  cache->can_insert = WTinyLFUv0_can_insert;
  cache->get_occupied_byte = WTinyLFUv0_get_occupied_byte;
  cache->get_n_obj = WTinyLFUv0_get_n_obj;

  cache->eviction_params = (WTinyLFUv0_params_t *)malloc(sizeof(WTinyLFUv0_params_t));
  WTinyLFUv0_params_t *WTinyLFUv0_params = (WTinyLFUv0_params_t *)(cache->eviction_params);

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = WTinyLFUv0_params->SLRU->obj_md_size;
    // TODO: not sure whether it works
  } else {
    cache->obj_md_size = 0;
  }

  if (cache_specific_params != NULL) {
    ERROR("%s does not support any parameters, but got %s\n", cache->cache_name,
          cache_specific_params);
    abort();
  } else {
    WTinyLFUv0_parse_params(cache, cache_specific_params);
    // TODO: to fix the name of function
  }

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size *= WTinyLFUv0_params->window_size;
  WTinyLFUv0_params->LRU = LRU_init(ccache_params_local, NULL);
#ifdef DEBUG_MODE
  printf("LRU cache size: %ld, consider_obj_metadata: %d, hashpower: %d\n",
  ccache_params_local.cache_size, ccache_params_local.consider_obj_metadata, ccache_params_local.hashpower);
  // printf("LRU obj md size: %d\n", WTinyLFUv0_params->LRU->obj_md_size);
#endif
  ccache_params_local.cache_size = ccache_params.cache_size;
  ccache_params_local.cache_size -= WTinyLFUv0_params->LRU->cache_size;
  WTinyLFUv0_params->SLRU = SLRU_init(ccache_params_local, "seg-size=4:1");

#ifdef DEBUG_MODE
  printf("SLRU cache size: %ld, consider_obj_metadata: %d, hashpower: %d\n\n",
  ccache_params_local.cache_size, ccache_params_local.consider_obj_metadata, ccache_params_local.hashpower);
#endif

  WTinyLFUv0_params->CBF = (struct minimalIncrementCBF *)malloc(sizeof(struct minimalIncrementCBF));
  assert(WTinyLFUv0_params->CBF != NULL);
  WTinyLFUv0_params->CBF->ready = 0;

  // TODO @ Ziyue: how to set entries and error rate?
  //int ret = minimalIncrementCBF_init(WTinyLFUv0_params->CBF, WTinyLFUv0_params->SLRU->cache_size / 2048, 0.001);
  int ret = minimalIncrementCBF_init(WTinyLFUv0_params->CBF, WTinyLFUv0_params->SLRU->cache_size, 0.001);
  if (ret != 0) {
    ERROR("CBF init failed\n");
  }

#ifdef DEBUG_MODE
  minimalIncrementCBF_print(WTinyLFUv0_params->CBF);
#endif

  WTinyLFUv0_params->request_counter = 0; // initialize request counter

  return cache;
}

static void WTinyLFUv0_free(cache_t *cache) {
  WTinyLFUv0_params_t *params = (WTinyLFUv0_params_t *)(cache->eviction_params);
  params->LRU->cache_free(params->LRU);
  params->SLRU->cache_free(params->SLRU);

  minimalIncrementCBF_free(params->CBF);
  free(params->CBF);

  cache_struct_free(cache);
}

static cache_obj_t * WTinyLFUv0_find(cache_t *cache, const request_t *req,
                  const bool update_cache) {
  WTinyLFUv0_params_t *WTinyLFUv0_params = (WTinyLFUv0_params_t *)(cache->eviction_params);
  assert(WTinyLFUv0_params != NULL);
  // static __thread request_t *req_local = NULL;
  // if (req_local == NULL) {
  //   req_local = new_request();
  // }

  cache_obj_t *obj = WTinyLFUv0_params->LRU->find(WTinyLFUv0_params->LRU, req, update_cache);
  bool cache_hit = (obj != NULL);
#ifdef DEBUG_MODE
  if(obj != NULL)
    printf("[find LRU hit] req id: %zu, obj id: %zu, obj size: %u, update_cache: %d\n",
      req->obj_id, obj->obj_id, obj->obj_size, update_cache);
  else
    printf("[find LRU miss] req id: %zu ", req->obj_id);
#endif
  if (cache_hit) {
#ifdef DEBUG_MODE
    printf("\n");
#endif
    return obj;
  }
  obj = WTinyLFUv0_params->SLRU->find(WTinyLFUv0_params->SLRU, req, update_cache);
  cache_hit = (obj != NULL);
#ifdef DEBUG_MODE
  if(obj != NULL)
    printf("-> [find SLRU hit] obj size: %u, SLRU occupied_byte: %zu, update_cache: %d\n",
      obj->obj_size, WTinyLFUv0_params->SLRU->occupied_byte, update_cache);
  else
    printf("-> [find SLRU miss]\n");
#endif
  if (cache_hit && update_cache) {
    // frequency update
    minimalIncrementCBF_add(WTinyLFUv0_params->CBF, (void *)&req->obj_id, 8);

#ifdef DEBUG_MODE
    printf("minimal Increment CBF add obj id %zu: %d\n\n", req->obj_id, minimalIncrementCBF_estimate(WTinyLFUv0_params->CBF, (void *)&req->obj_id, 8));
#endif
    WTinyLFUv0_params->request_counter++;
    if (WTinyLFUv0_params->request_counter >= WTinyLFUv0_params->max_request_num) {
      WTinyLFUv0_params->request_counter = 0;
      minimalIncrementCBF_decay(WTinyLFUv0_params->CBF);
    }
    return obj;
  } else if (cache_hit) {
#ifdef DEBUG_MODE
  printf("\n");
#endif
    return obj;
  }

#ifdef DEBUG_MODE
  printf("\n");
#endif

  return NULL;
}

static bool WTinyLFUv0_get(cache_t *cache, const request_t *req) {
  /* because this field cannot be updated in time since segment LRUs are
   * updated, so we should not use this field */
  DEBUG_ASSERT(cache->occupied_byte == 0);

  bool ck = cache_get_base(cache, req);
  return ck;
}

cache_obj_t *WTinyLFUv0_insert(cache_t *cache, const request_t *req) {
  WTinyLFUv0_params_t *WTinyLFUv0_params = (WTinyLFUv0_params_t *)(cache->eviction_params);

#ifdef DEBUG_MODE
  printf("[insert] req obj_id: %zu, req obj_size: %zu, LRU occupied_byte: %zu (capacity: %zu)\n",
    req->obj_id, req->obj_size, WTinyLFUv0_params->LRU->occupied_byte, WTinyLFUv0_params->LRU->cache_size);
#endif

  while(WTinyLFUv0_params->LRU->occupied_byte + req->obj_size + cache->obj_md_size >
      WTinyLFUv0_params->LRU->cache_size) {
    WTinyLFUv0_evict(cache, req);
#ifdef DEBUG_MODE
    printf("[after evict LRU] LRU occupied_byte: %zu (capacity: %zu)\n", WTinyLFUv0_params->LRU->occupied_byte, WTinyLFUv0_params->LRU->cache_size); 
#endif
  }
#ifdef DEBUG_MODE
  printf("\n");
#endif

  return WTinyLFUv0_params->LRU->insert(WTinyLFUv0_params->LRU, req);
}

static cache_obj_t *WTinyLFUv0_to_evict(cache_t *cache, const request_t *req) {
  // Warning: don't use this function
  assert(false);
}

static void WTinyLFUv0_evict(cache_t *cache, const request_t *req) {
  WTinyLFUv0_params_t *WTinyLFUv0_params = (WTinyLFUv0_params_t *)(cache->eviction_params);

  if(WTinyLFUv0_params->LRU->occupied_byte > 0) {
    cache_obj_t *window_victim = WTinyLFUv0_params->LRU->to_evict(WTinyLFUv0_params->LRU, req);
    assert(window_victim != NULL);
  #ifdef DEBUG_MODE
    printf("[to evict LRU] window_victim obj_id: %zu, obj_size: %u, LRU occupied_byte: %zu (capacity: %zu)\n",
      window_victim->obj_id, window_victim->obj_size, WTinyLFUv0_params->LRU->occupied_byte, WTinyLFUv0_params->LRU->cache_size);
  #endif

    // window victim req is different from req
    static __thread request_t *req_local = NULL;
    if (req_local == NULL) {
      req_local = new_request();
    }
    copy_cache_obj_to_request(req_local, window_victim);

    /** only when SLRU is full, evict an obj from the SLRU **/

    // if SLRU has enough space, insert the obj into SLRU
    if(WTinyLFUv0_params->SLRU->occupied_byte + req_local->obj_size + cache->obj_md_size <=
        WTinyLFUv0_params->SLRU->cache_size) {
  #ifdef DEBUG_MODE
      printf("[insert SLRU before] req obj_id: %zu, req obj_size: %zu, SLRU occupied_byte: %zu (capacity: %zu)\n",
        req_local->obj_id, req_local->obj_size, WTinyLFUv0_params->SLRU->occupied_byte, WTinyLFUv0_params->SLRU->cache_size);
  #endif
      cache_obj_t *obj_temp = WTinyLFUv0_params->SLRU->insert(WTinyLFUv0_params->SLRU, req_local);
      DEBUG_ASSERT(obj_temp != NULL);
  #ifdef DEBUG_MODE
      printf("[insert SLRU after] req obj_id: %zu, req obj_size: %zu, SLRU occupied_byte: %zu (capacity: %zu)\n",
        req_local->obj_id, req_local->obj_size, WTinyLFUv0_params->SLRU->occupied_byte, WTinyLFUv0_params->SLRU->cache_size);
  #endif
      WTinyLFUv0_params->LRU->remove(WTinyLFUv0_params->LRU, window_victim->obj_id);
    } else {
      // compare the frequency of window_victim and slru_victim
      cache_obj_t *slru_victim = WTinyLFUv0_params->SLRU->to_evict(WTinyLFUv0_params->SLRU, req_local);
      DEBUG_ASSERT(slru_victim != NULL);
      // if window_victim is more frequent, insert it into SLRU
  #ifdef DEBUG_MODE
      printf("[compare] window_victim obj_id %zu -> %d, slru_victim obj_id %zu -> %d\n",
      window_victim->obj_id,
      minimalIncrementCBF_estimate(WTinyLFUv0_params->CBF, (void *)&window_victim->obj_id, sizeof(window_victim->obj_id)),
      slru_victim->obj_id,
      minimalIncrementCBF_estimate(WTinyLFUv0_params->CBF, (void *)&slru_victim->obj_id, sizeof(slru_victim->obj_id))
      );
  #endif
      if(minimalIncrementCBF_estimate(WTinyLFUv0_params->CBF, (void *)&window_victim->obj_id, sizeof(window_victim->obj_id)) >
          minimalIncrementCBF_estimate(WTinyLFUv0_params->CBF, (void *)&slru_victim->obj_id, sizeof(slru_victim->obj_id))) {
        WTinyLFUv0_params->SLRU->evict(WTinyLFUv0_params->SLRU, req_local);
  #ifdef DEBUG_MODE
        printf("[evict SLRU victim] obj_id: %zu, obj_size: %u, SLRU occupied_byte: %zu (capacity: %zu)\n",
          slru_victim->obj_id, slru_victim->obj_size, WTinyLFUv0_params->SLRU->occupied_byte, WTinyLFUv0_params->SLRU->cache_size);
  #endif
        bool ret = WTinyLFUv0_params->LRU->remove(WTinyLFUv0_params->LRU, window_victim->obj_id);
        assert(ret);

        cache_obj_t* cache_obj = WTinyLFUv0_params->SLRU->insert(WTinyLFUv0_params->SLRU, req_local);
        assert(cache_obj != NULL);
      } else {
  #ifdef DEBUG_MODE
        printf("[evict LRU victim] obj_id: %zu\n", window_victim->obj_id);
  #endif
        WTinyLFUv0_params->LRU->evict(WTinyLFUv0_params->LRU, req);
      }
    }
    // TODO @ Ziyue: add doorkeeper
    minimalIncrementCBF_add(WTinyLFUv0_params->CBF, (void *)(&window_victim->obj_id), 8);
  #ifdef DEBUG_MODE
    printf("minimal Increment CBF add obj id %zu: %d\n\n", window_victim->obj_id, minimalIncrementCBF_estimate(WTinyLFUv0_params->CBF, (void *)(&window_victim->obj_id), 8));
  #endif
  } else if(WTinyLFUv0_params->LRU->occupied_byte == 0) {
    WTinyLFUv0_params->SLRU->evict(WTinyLFUv0_params->SLRU, req);
  }
  else {
#ifdef DEBUG_MODE
    printf("error evict: LRU and SLRU are both empty\n");
#endif
    exit(1);
  }
}

static bool WTinyLFUv0_remove(cache_t *cache, const obj_id_t obj_id) {

  WTinyLFUv0_params_t *params = (WTinyLFUv0_params_t *)(cache->eviction_params);
  if (params->LRU->remove(params->LRU, obj_id)) {
    return true;
  }
  if (params->SLRU->remove(params->SLRU, obj_id)) {
    return true;
  }
  return false;
}

#ifdef __cplusplus
extern "C"
}
#endif
