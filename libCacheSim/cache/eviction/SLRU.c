//
//  a LRU module that supports different obj size
//
//  SLRU.c
//  libCacheSim
//
//

#include "../include/libCacheSim/evictionAlgo/SLRU.h"

#include "../dataStructure/hashtable/hashtable.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SLRU_params {
  cache_t **LRUs;
  int n_seg;
} SLRU_params_t;

const char *SLRU_default_init_params(void) { return "n_seg=4;"; }

cache_t *SLRU_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("SLRU", ccache_params);
  cache->cache_init = SLRU_init;
  cache->cache_free = SLRU_free;
  cache->get = SLRU_get;
  cache->check = SLRU_check;
  cache->insert = SLRU_insert;
  cache->evict = SLRU_evict;
  cache->remove = SLRU_remove;
  cache->to_evict = SLRU_to_evict;
  cache->init_params = cache_specific_params;

  cache->eviction_params = (SLRU_params_t *)malloc(sizeof(SLRU_params_t));
  SLRU_params_t *SLRU_params = (SLRU_params_t *)(cache->eviction_params);
  SLRU_params->n_seg = 4;

  if (cache_specific_params != NULL) {
    char *params_str = strdup(cache_specific_params);

    while (params_str != NULL && params_str[0] != '\0') {
      char *key = strsep((char **)&params_str, "=");
      char *value = strsep((char **)&params_str, ";");
      if (strcasecmp(key, "n_seg") == 0) {
        SLRU_params->n_seg = atoi(value);
      } else {
        ERROR("%s does not have parameter %s\n", cache->cache_name, key);
        exit(1);
      }
    }
    free(params_str);
  }

  SLRU_params->LRUs =
      (cache_t **)malloc(sizeof(cache_t *) * SLRU_params->n_seg);

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size /= SLRU_params->n_seg;
  for (int i = 0; i < SLRU_params->n_seg; i++) {
    SLRU_params->LRUs[i] = LRU_init(ccache_params_local, NULL);
  }

  return cache;
}

void SLRU_free(cache_t *cache) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *)(cache->eviction_params);
  int i;
  for (i = 0; i < SLRU_params->n_seg; i++) LRU_free(SLRU_params->LRUs[i]);
  free(SLRU_params->LRUs);
  cache_struct_free(cache);
}

/**
 * @brief move an object from ith LRU into (i-1)th LRU, cool
 * (i-1)th LRU if it is full
 *
 * @param cache
 * @param i
 */
void SLRU_cool(cache_t *cache, int i) {
  cache_obj_t evicted_obj;
  SLRU_params_t *SLRU_params = (SLRU_params_t *)(cache->eviction_params);
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }
  LRU_evict(SLRU_params->LRUs[i], NULL, &evicted_obj);

  if (i == 0) return;

  // If lower LRUs are full
  while (SLRU_params->LRUs[i - 1]->occupied_size + evicted_obj.obj_size +
             cache->per_obj_overhead >
         SLRU_params->LRUs[i - 1]->cache_size)
    SLRU_cool(cache, i - 1);

  copy_cache_obj_to_request(req_local, &evicted_obj);
  LRU_insert(SLRU_params->LRUs[i - 1], req_local);
}

cache_ck_res_e SLRU_check(cache_t *cache, const request_t *req,
                          const bool update_cache) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *)(cache->eviction_params);
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  for (int i = 0; i < SLRU_params->n_seg; i++) {
    cache_ck_res_e ret = LRU_check(SLRU_params->LRUs[i], req, update_cache);

    if (ret == cache_ck_hit) {
      // bump object from lower segment to upper segment;
      if (i != SLRU_params->n_seg - 1) {
        LRU_remove(SLRU_params->LRUs[i], req->obj_id);
        // obj_id_t evicted_obj_id = req->obj_id;

        // If the upper LRU is full;
        while (SLRU_params->LRUs[i + 1]->occupied_size + req->obj_size +
                   cache->per_obj_overhead >
               SLRU_params->LRUs[i + 1]->cache_size)
          SLRU_cool(cache, i + 1);

        // req->obj_id = evicted_obj_id;
        LRU_insert(SLRU_params->LRUs[i + 1], req);
      }
      return cache_ck_hit;
    } else if (ret == cache_ck_expired)
      return cache_ck_expired;
  }
  return cache_ck_miss;
}

cache_ck_res_e SLRU_get(cache_t *cache, const request_t *req) {
  cache_obj_t *cache_obj;
  SLRU_params_t *SLRU_params = (SLRU_params_t *)(cache->eviction_params);
  cache_ck_res_e ret;
  ret = SLRU_check(cache, req, true);

  if (ret == cache_ck_miss || ret == cache_ck_expired) {
    if (req->obj_size + cache->per_obj_overhead > cache->cache_size) {
      return ret;
    }
    SLRU_insert(cache, req);
  }
  return ret;
}

void SLRU_insert(cache_t *cache, const request_t *req) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *)(cache->eviction_params);

  int i;

  // Find the lowest LRU with space for insertion
  for (i = 0; i < SLRU_params->n_seg; i++) {
    if (SLRU_params->LRUs[i]->occupied_size + req->obj_size +
            cache->per_obj_overhead <=
        SLRU_params->LRUs[i]->cache_size) {
      LRU_insert(SLRU_params->LRUs[i], req);
      return;
    }
  }

  // If all LRUs are filled, evict an obj from the lowest LRU.
  if (i == SLRU_params->n_seg) {
    while (SLRU_params->LRUs[0]->occupied_size + req->obj_size +
               cache->per_obj_overhead >
           SLRU_params->LRUs[0]->cache_size) {
      SLRU_evict(cache, req, NULL);
    }
    LRU_insert(SLRU_params->LRUs[0], req);
  }
}

cache_obj_t *SLRU_to_evict(cache_t *cache) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *)(cache->eviction_params);
  return SLRU_params->LRUs[0]->to_evict(SLRU_params->LRUs[0]);
}

void SLRU_evict(cache_t *cache, const request_t *req,
                cache_obj_t *evicted_obj) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *)(cache->eviction_params);
  cache_evict_LRU(SLRU_params->LRUs[0], req, evicted_obj);
}

void SLRU_remove(cache_t *cache, const obj_id_t obj_id) {
  SLRU_params_t *SLRU_params = (SLRU_params_t *)(cache->eviction_params);
  cache_obj_t *obj;
  for (int i = 0; i < SLRU_params->n_seg; i++) {
    obj = cache_get_obj_by_id(SLRU_params->LRUs[i], obj_id);
    if (obj) {
      remove_obj_from_list(&(SLRU_params->LRUs[i])->q_head,
                           &(SLRU_params->LRUs[i])->q_tail, obj);
      cache_remove_obj_base(SLRU_params->LRUs[i], obj);
      return;
    }
  }
  if (obj == NULL) {
    WARN("obj (%" PRIu64 ") to remove is not in the cache\n", obj_id);
    return;
  }
}

#ifdef __cplusplus
extern "C" {
#endif
