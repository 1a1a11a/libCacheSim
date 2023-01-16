//
//  a novel approximate LFU structure (TinyLFU) admission control
//  any eviction policy
//  combined with a windowed LRU structure (W-TinyLFU)
//  to resist burst traffic of temporal hot objects
//
//  NOTE: it is NOT thread-safe
//
//  part of the details borrowed from https://github.com/mandreyel/w-tinylfu
//
//  WTinyLFUv1.c
//  libCacheSim
//
//  Created by Ziyue on 14/1/2023.
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"

#include "../../../dataStructure/minimalIncrementCBF.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_MODE
#undef DEBUG_MODE
#define DEBUG_MODE_2
#undef DEBUG_MODE_2

typedef struct WTinyLFUv1_params {
  cache_t *LRU;  // LRU as windowed LRU
  cache_t *main_cache; // any eviction policy
  double window_size;
  struct minimalIncrementCBF * CBF;
  size_t max_request_num;
  size_t request_counter;
  char main_cache_type[32];
  bool is_windowed;
} WTinyLFUv1_params_t;

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

static void WTinyLFUv1_free(cache_t *cache);
static cache_obj_t * WTinyLFUv1_find(cache_t *cache, const request_t *req, const bool update);
static bool WTinyLFUv1_get(cache_t *cache, const request_t *req);
static cache_obj_t *WTinyLFUv1_insert(cache_t *cache, const request_t *req);
static cache_obj_t *WTinyLFUv1_to_evict(cache_t *cache, const request_t *req);
static void WTinyLFUv1_evict(cache_t *cache, const request_t *req);
static bool WTinyLFUv1_remove(cache_t *cache, const obj_id_t obj_id);



static void WTinyLFUv1_parse_params(cache_t *cache,
                                    const char *cache_specific_params) {
  WTinyLFUv1_params_t *params = (WTinyLFUv1_params_t *)cache->eviction_params;

  // by default
  params->window_size = 0.01;
  params->is_windowed = true;

  //params->max_request_num = 32 * cache->cache_size; // 32 * cache_size

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;
  char *end;
  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    // if (strcasecmp(key, "fifo-size-ratio") == 0) {
    //   params->fifo_size_ratio = strtod(value, NULL);
    // } else if (strcasecmp(key, "main-cache") == 0) {
    //   strncpy(params->main_cache_type, value, 30);
    // } else if (strcasecmp(key, "print") == 0) {
    //   printf("parameters: %s\n", QDLPv1_current_params(params));
    //   exit(0);
    if(strcasecmp(key, "main-cache") == 0) {
      strncpy(params->main_cache_type, value, 30);
    } else if(strcasecmp(key, "window-size") == 0) {
      params->window_size = strtod(value, NULL); // cover default value
      if(params->window_size < 0 || params->window_size >= 1) {
        ERROR("window_size must be in [0, 1)\n");
        exit(1);
      }
      params->is_windowed = (params->window_size > 0);
    }
    else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }
  return;
}

/* WTinyLFUv1 cannot an object larger than segment size */
bool WTinyLFUv1_can_insert(cache_t *cache, const request_t *req) {
  WTinyLFUv1_params_t *params = (WTinyLFUv1_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);

  #ifdef DEBUG_MODE
  if(params->is_windowed == false) {
    if(params->main_cache->can_insert(params->main_cache, req) == false) {
      printf("[can_insert FALSE] req object size: %lu, cache object md size: %u, main_cache can insert: %d\n",
        req->obj_size, cache->obj_md_size, params->main_cache->can_insert(params->main_cache, req));
      DEBUG_ASSERT(false);
    }
  } else if(can_insert &&
      (req->obj_size + cache->obj_md_size <= params->LRU->cache_size) &&
      (params->main_cache->can_insert(params->main_cache, req)) == false) {
    printf("[can_insert FALSE] req object size: %lu, cache object md size: %u, cache size: %lu, main_cache can insert: %d\n",
      req->obj_size, cache->obj_md_size, params->LRU->cache_size, params->main_cache->can_insert(params->main_cache, req));
    DEBUG_ASSERT(false);
  }
  #endif

  if(params->is_windowed == false)
    return can_insert && params->main_cache->can_insert(params->main_cache, req);
  else
    return can_insert &&
          (req->obj_size + cache->obj_md_size <= params->LRU->cache_size) &&
            (params->main_cache->can_insert(params->main_cache, req));
}

int64_t WTinyLFUv1_get_occupied_byte(const cache_t *cache) {
  WTinyLFUv1_params_t *params = (WTinyLFUv1_params_t *)cache->eviction_params;
  int64_t occupied_byte = 0;

  if(params->is_windowed == false)
    return params->main_cache->get_occupied_byte(params->main_cache);

  occupied_byte += params->LRU->occupied_byte;
  occupied_byte += params->main_cache->get_occupied_byte(params->main_cache);

#ifdef DEBUG_MODE
  printf("[get_occupied_byte] LRU: %ld, main_cache: %ld\n\n",
    params->LRU->occupied_byte, params->main_cache->get_occupied_byte(params->main_cache));
#endif

  return occupied_byte;
}

int64_t WTinyLFUv1_get_n_obj(const cache_t *cache) {
  WTinyLFUv1_params_t *params = (WTinyLFUv1_params_t *)cache->eviction_params;
  int64_t n_obj = 0;

  if(params->is_windowed == false)
    return params->main_cache->get_n_obj(params->main_cache);

  n_obj += params->LRU->n_obj;
  n_obj += params->main_cache->get_n_obj(params->main_cache);
  return n_obj;
}

cache_t *WTinyLFUv1_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("WTinyLFUv1", ccache_params);
  cache->cache_init = WTinyLFUv1_init;
  cache->cache_free = WTinyLFUv1_free;
  cache->get = WTinyLFUv1_get;
  cache->find = WTinyLFUv1_find;
  cache->insert = WTinyLFUv1_insert;
  cache->evict = WTinyLFUv1_evict;
  cache->remove = WTinyLFUv1_remove;
  cache->to_evict = WTinyLFUv1_to_evict;
  cache->init_params = cache_specific_params;
  cache->can_insert = WTinyLFUv1_can_insert;
  cache->get_occupied_byte = WTinyLFUv1_get_occupied_byte;
  cache->get_n_obj = WTinyLFUv1_get_n_obj;

  cache->eviction_params = (WTinyLFUv1_params_t *)malloc(sizeof(WTinyLFUv1_params_t));
  WTinyLFUv1_params_t *params = (WTinyLFUv1_params_t *)(cache->eviction_params);

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = params->main_cache->obj_md_size;
    // TODO: not sure whether it works
  } else {
    cache->obj_md_size = 0;
  }

  WTinyLFUv1_parse_params(cache, cache_specific_params);

  common_cache_params_t ccache_params_local = ccache_params;

  if(params->is_windowed){
    ccache_params_local.cache_size *= params->window_size;
    params->LRU = LRU_init(ccache_params_local, NULL);
#ifdef DEBUG_MODE
    printf("LRU cache size: %ld, consider_obj_metadata: %d, hashpower: %d\n",
    ccache_params_local.cache_size, ccache_params_local.consider_obj_metadata, ccache_params_local.hashpower);
#endif
    ccache_params_local.cache_size = ccache_params.cache_size;
    ccache_params_local.cache_size -= params->LRU->cache_size;
  } else {
    params->LRU = NULL;
  }

  
  if (strcasecmp(params->main_cache_type, "LRU") == 0) {
    params->main_cache = LRU_init(ccache_params_local, NULL);
  } else if(strcasecmp(params->main_cache_type, "SLRU") == 0) {
    params->main_cache = SLRU_init(ccache_params_local, "seg-size=4:1");
  } else if(strcasecmp(params->main_cache_type, "LFU") == 0) {
    params->main_cache = LFU_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "ARC") == 0) {
    params->main_cache = ARC_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "clock") == 0) {
    params->main_cache = Clock_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "myclock") == 0) {
    params->main_cache = MyClock_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "LeCaR") == 0) {
    params->main_cache = LeCaR_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->main_cache_type, "Cacheus") == 0) {
    params->main_cache = Cacheus_init(ccache_params_local, NULL);
  }
  // else if (strcasecmp(params->main_cache_type, "Hyperbolic") == 0) {
  //   params->main_cache = Hyperbolic_init(ccache_params_local, NULL);
  // }
  else if (strcasecmp(params->main_cache_type, "LHD") == 0) {
    params->main_cache = LHD_init(ccache_params_local, NULL);
  }
  else {
    ERROR("WTinyLFUv1 does not support %s \n", params->main_cache_type);
  }

  params->max_request_num = 32 * params->main_cache->cache_size; // sample size is 32

#ifdef DEBUG_MODE
  printf("main_cache cache size: %ld, consider_obj_metadata: %d, hashpower: %d\n\n",
  ccache_params_local.cache_size, ccache_params_local.consider_obj_metadata, ccache_params_local.hashpower);
#endif

  params->CBF = (struct minimalIncrementCBF *)malloc(sizeof(struct minimalIncrementCBF));
  DEBUG_ASSERT(params->CBF != NULL);
  params->CBF->ready = 0;

  // TODO @ Ziyue: how to set entries and error rate?
  int ret = minimalIncrementCBF_init(params->CBF, params->main_cache->cache_size, 0.001);
  if (ret != 0) {
    ERROR("CBF init failed\n");
  }

#ifdef DEBUG_MODE
  minimalIncrementCBF_print(params->CBF);
#endif

  params->request_counter = 0; // initialize request counter

  return cache;
}

static void WTinyLFUv1_free(cache_t *cache) {
  WTinyLFUv1_params_t *params = (WTinyLFUv1_params_t *)(cache->eviction_params);
  if(params->is_windowed)
    params->LRU->cache_free(params->LRU);
  params->main_cache->cache_free(params->main_cache);

  minimalIncrementCBF_free(params->CBF);
  free(params->CBF);

  cache_struct_free(cache);
}

static cache_obj_t * WTinyLFUv1_find(cache_t *cache, const request_t *req,
                  const bool update_cache) {
  WTinyLFUv1_params_t *WTinyLFUv1_params = (WTinyLFUv1_params_t *)(cache->eviction_params);
#ifdef DEBUG_MODE
  if(WTinyLFUv1_params == NULL)
    printf("cache->eviction_params == NULL\n");
  DEBUG_ASSERT(WTinyLFUv1_params != NULL);
#endif

  if(WTinyLFUv1_params->is_windowed){
    cache_obj_t *obj = WTinyLFUv1_params->LRU->find(WTinyLFUv1_params->LRU, req, update_cache);
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
  }
  
  cache_obj_t *obj = WTinyLFUv1_params->main_cache->find(WTinyLFUv1_params->main_cache, req, update_cache);
  bool cache_hit = (obj != NULL);
#ifdef DEBUG_MODE_2
  if(obj != NULL)
    printf("-> [find main_cache hit] obj size: %u, main_cache occupied_byte: %zu, update_cache: %d\n",
      obj->obj_size, WTinyLFUv1_params->main_cache->get_occupied_byte(WTinyLFUv1_params->main_cache), update_cache);
  else
    printf("-> [find main_cache miss]\n");
#endif
  if (cache_hit && update_cache) {
    // frequency update
    minimalIncrementCBF_add(WTinyLFUv1_params->CBF, (void *)&req->obj_id, 8);

#ifdef DEBUG_MODE
    printf("minimal Increment CBF add obj id %zu: %d\n\n", req->obj_id, minimalIncrementCBF_estimate(WTinyLFUv1_params->CBF, (void *)&req->obj_id, 8));
#endif
    WTinyLFUv1_params->request_counter++;
    if (WTinyLFUv1_params->request_counter >= WTinyLFUv1_params->max_request_num) {
      WTinyLFUv1_params->request_counter = 0;
      minimalIncrementCBF_decay(WTinyLFUv1_params->CBF);
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

static bool WTinyLFUv1_get(cache_t *cache, const request_t *req) {
  /* because this field cannot be updated in time since segment LRUs are
   * updated, so we should not use this field */
  DEBUG_ASSERT(cache->occupied_byte == 0);

  bool ck = cache_get_base(cache, req);
  return ck;
}

cache_obj_t *WTinyLFUv1_insert(cache_t *cache, const request_t *req) {
  WTinyLFUv1_params_t *WTinyLFUv1_params = (WTinyLFUv1_params_t *)(cache->eviction_params);

  if(WTinyLFUv1_params->is_windowed){
#ifdef DEBUG_MODE
    printf("[insert] req obj_id: %zu, req obj_size: %zu, LRU occupied_byte: %zu (capacity: %zu)\n",
      req->obj_id, req->obj_size, WTinyLFUv1_params->LRU->occupied_byte, WTinyLFUv1_params->LRU->cache_size);
#endif

    while(WTinyLFUv1_params->LRU->occupied_byte + req->obj_size + cache->obj_md_size >
      WTinyLFUv1_params->LRU->cache_size) {
      WTinyLFUv1_evict(cache, req);
#ifdef DEBUG_MODE
      printf("[after evict LRU] LRU occupied_byte: %zu (capacity: %zu)\n", WTinyLFUv1_params->LRU->occupied_byte, WTinyLFUv1_params->LRU->cache_size); 
#endif
    }
#ifdef DEBUG_MODE
    printf("\n");
#endif
    return WTinyLFUv1_params->LRU->insert(WTinyLFUv1_params->LRU, req);
  } else {
    while(WTinyLFUv1_params->main_cache->get_occupied_byte(WTinyLFUv1_params->main_cache) + req->obj_size + WTinyLFUv1_params->main_cache->obj_md_size >
      WTinyLFUv1_params->main_cache->cache_size) {
      WTinyLFUv1_params->main_cache->evict(WTinyLFUv1_params->main_cache, req);
    }
    return WTinyLFUv1_params->main_cache->insert(WTinyLFUv1_params->main_cache, req);
  }
}

static cache_obj_t *WTinyLFUv1_to_evict(cache_t *cache, const request_t *req) {
  // Warning: don't use this function
  DEBUG_ASSERT(false);
}

static void WTinyLFUv1_evict(cache_t *cache, const request_t *req) {
  WTinyLFUv1_params_t *WTinyLFUv1_params = (WTinyLFUv1_params_t *)(cache->eviction_params);

  if(WTinyLFUv1_params->is_windowed == false) {
    WTinyLFUv1_params->main_cache->evict(WTinyLFUv1_params->main_cache, req);
    return;
  }

  if(WTinyLFUv1_params->LRU->occupied_byte > 0) {
    cache_obj_t *window_victim = WTinyLFUv1_params->LRU->to_evict(WTinyLFUv1_params->LRU, req);
    DEBUG_ASSERT(window_victim != NULL);
  #ifdef DEBUG_MODE
    printf("[to evict LRU] window_victim obj_id: %zu, obj_size: %u, LRU occupied_byte: %zu (capacity: %zu)\n",
      window_victim->obj_id, window_victim->obj_size, WTinyLFUv1_params->LRU->occupied_byte, WTinyLFUv1_params->LRU->cache_size);
  #endif

    // window victim req is different from req
    static __thread request_t *req_local = NULL;
    if (req_local == NULL) {
      req_local = new_request();
    }
    copy_cache_obj_to_request(req_local, window_victim);

    /** only when main_cache is full, evict an obj from the main_cache **/

    // if main_cache has enough space, insert the obj into main_cache
    if(WTinyLFUv1_params->main_cache->get_occupied_byte(WTinyLFUv1_params->main_cache) + req_local->obj_size + cache->obj_md_size <=
        WTinyLFUv1_params->main_cache->cache_size) {
  #ifdef DEBUG_MODE
      printf("[insert main_cache before] req obj_id: %zu, req obj_size: %zu, main_cache occupied_byte: %zu (capacity: %zu)\n",
        req_local->obj_id, req_local->obj_size, WTinyLFUv1_params->main_cache->get_occupied_byte(WTinyLFUv1_params->main_cache), WTinyLFUv1_params->main_cache->cache_size);
  #endif
      cache_obj_t *obj_temp = WTinyLFUv1_params->main_cache->insert(WTinyLFUv1_params->main_cache, req_local);
      //DEBUG_ASSERT(obj_temp != NULL);
  #ifdef DEBUG_MODE
      printf("[insert main_cache after] req obj_id: %zu, req obj_size: %zu, main_cache occupied_byte: %zu (capacity: %zu)\n",
        req_local->obj_id, req_local->obj_size, WTinyLFUv1_params->main_cache->get_occupied_byte(WTinyLFUv1_params->main_cache), WTinyLFUv1_params->main_cache->cache_size);
  #endif
      WTinyLFUv1_params->LRU->remove(WTinyLFUv1_params->LRU, window_victim->obj_id);
    } else {
      // compare the frequency of window_victim and main_cache_victim
      cache_obj_t *main_cache_victim = WTinyLFUv1_params->main_cache->to_evict(WTinyLFUv1_params->main_cache, req_local);
// #ifdef DEBUG_MODE
//       if(main_cache_victim == NULL) {
//         printf("main_cache_victim is NULL\n");
//         exit(1);
//       }
// #endif
      DEBUG_ASSERT(main_cache_victim != NULL);
      // if window_victim is more frequent, insert it into main_cache
  #ifdef DEBUG_MODE
      printf("[compare] window_victim obj_id %zu -> %d, main_cache_victim obj_id %zu -> %d\n",
      window_victim->obj_id,
      minimalIncrementCBF_estimate(WTinyLFUv1_params->CBF, (void *)&window_victim->obj_id, sizeof(window_victim->obj_id)),
      main_cache_victim->obj_id,
      minimalIncrementCBF_estimate(WTinyLFUv1_params->CBF, (void *)&main_cache_victim->obj_id, sizeof(main_cache_victim->obj_id))
      );
  #endif
      if(minimalIncrementCBF_estimate(WTinyLFUv1_params->CBF, (void *)&window_victim->obj_id, sizeof(window_victim->obj_id)) >
          minimalIncrementCBF_estimate(WTinyLFUv1_params->CBF, (void *)&main_cache_victim->obj_id, sizeof(main_cache_victim->obj_id))) {
        WTinyLFUv1_params->main_cache->evict(WTinyLFUv1_params->main_cache, req_local);
  #ifdef DEBUG_MODE
        printf("[evict main_cache victim] obj_id: %zu, obj_size: %u, main_cache occupied_byte: %zu (capacity: %zu)\n",
          main_cache_victim->obj_id, main_cache_victim->obj_size, WTinyLFUv1_params->main_cache->get_occupied_byte(WTinyLFUv1_params->main_cache), WTinyLFUv1_params->main_cache->cache_size);
  #endif
        bool ret = WTinyLFUv1_params->LRU->remove(WTinyLFUv1_params->LRU, window_victim->obj_id);
        DEBUG_ASSERT(ret);

        cache_obj_t* cache_obj = WTinyLFUv1_params->main_cache->insert(WTinyLFUv1_params->main_cache, req_local);
        //DEBUG_ASSERT(cache_obj != NULL);
      } else {
  #ifdef DEBUG_MODE
        printf("[evict LRU victim] obj_id: %zu\n", window_victim->obj_id);
  #endif
        WTinyLFUv1_params->LRU->evict(WTinyLFUv1_params->LRU, req);
      }
    }
    // TODO @ Ziyue: add doorkeeper
    minimalIncrementCBF_add(WTinyLFUv1_params->CBF, (void *)(&window_victim->obj_id), 8);
  #ifdef DEBUG_MODE
    printf("minimal Increment CBF add obj id %zu: %d\n\n", window_victim->obj_id, minimalIncrementCBF_estimate(WTinyLFUv1_params->CBF, (void *)(&window_victim->obj_id), 8));
  #endif
  } else if(WTinyLFUv1_params->LRU->occupied_byte == 0) {
    WTinyLFUv1_params->main_cache->evict(WTinyLFUv1_params->main_cache, req);
  }
  else {
#ifdef DEBUG_MODE
    printf("error evict: LRU and main_cache are both empty\n");
#endif
    exit(1);
  }
}

static bool WTinyLFUv1_remove(cache_t *cache, const obj_id_t obj_id) {

  WTinyLFUv1_params_t *params = (WTinyLFUv1_params_t *)(cache->eviction_params);
  if (params->is_windowed && params->LRU->remove(params->LRU, obj_id)) {
    return true;
  }
  if (params->main_cache->remove(params->main_cache, obj_id)) {
    return true;
  }
  return false;
}

#ifdef __cplusplus
extern "C"
}
#endif
