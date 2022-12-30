//
//  a LRU module that supports different obj size
//
//  SLRUv0.c
//  libCacheSim
//
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/SLRUv0.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SLRUv0_params {
  cache_t **LRUs;
  int n_seg;
} SLRUv0_params_t;

static const char *SLRUv0_current_params(SLRUv0_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-seg=%d\n", params->n_seg);
  return params_str;
}

static void SLRUv0_parse_params(cache_t *cache,
                                const char *cache_specific_params) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)cache->eviction_params;
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

    if (strcasecmp(key, "n-seg") == 0) {
      params->n_seg = (int)strtol(value, &end, 0);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }

    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", SLRUv0_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }
  free(old_params_str);
}

/* SLRUv0 cannot an object larger than segment size */
bool SLRUv0_can_insert(cache_t *cache, const request_t *req) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);
  return can_insert &&
         (req->obj_size + cache->obj_md_size <= params->LRUs[0]->cache_size);
}

int64_t SLRUv0_get_occupied_byte(const cache_t *cache) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)cache->eviction_params;
  int64_t occupied_byte = 0;
  for (int i = 0; i < params->n_seg; i++) {
    occupied_byte += params->LRUs[i]->occupied_size;
  }
  return occupied_byte;
}

int64_t SLRUv0_get_n_obj(const cache_t *cache) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)cache->eviction_params;
  int64_t n_obj = 0;
  for (int i = 0; i < params->n_seg; i++) {
    n_obj += params->LRUs[i]->n_obj;
  }
  return n_obj;
}
static void SLRUv0_print_cache(cache_t *cache) {
  SLRUv0_params_t *params = (SLRUv0_params_t *)cache->eviction_params;
  for (int i = params->n_seg - 1; i >= 0; i--) {
    cache_obj_t *obj = params->LRUs[i]->q_head;
    while (obj) {
      printf("%ld(%u)->", obj->obj_id, obj->obj_size);
      obj = obj->queue.next;
    }
    printf(" | ");
  }
  printf("\n");
}

cache_t *SLRUv0_init(const common_cache_params_t ccache_params,
                     const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("SLRUv0", ccache_params);
  cache->cache_init = SLRUv0_init;
  cache->cache_free = SLRUv0_free;
  cache->get = SLRUv0_get;
  cache->check = SLRUv0_check;
  cache->insert = SLRUv0_insert;
  cache->evict = SLRUv0_evict;
  cache->remove = SLRUv0_remove;
  cache->to_evict = SLRUv0_to_evict;
  cache->init_params = cache_specific_params;
  cache->can_insert = SLRUv0_can_insert;
  cache->get_occupied_byte = SLRUv0_get_occupied_byte;
  cache->get_n_obj = SLRUv0_get_n_obj;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = (SLRUv0_params_t *)malloc(sizeof(SLRUv0_params_t));
  SLRUv0_params_t *SLRUv0_params = (SLRUv0_params_t *)(cache->eviction_params);
  SLRUv0_params->n_seg = 4;

  if (cache_specific_params != NULL) {
    SLRUv0_parse_params(cache, cache_specific_params);
  }

  SLRUv0_params->LRUs =
      (cache_t **)malloc(sizeof(cache_t *) * SLRUv0_params->n_seg);

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size /= SLRUv0_params->n_seg;
  ccache_params_local.hashpower /= MIN(16, ccache_params_local.hashpower - 4);
  for (int i = 0; i < SLRUv0_params->n_seg; i++) {
    SLRUv0_params->LRUs[i] = LRU_init(ccache_params_local, NULL);
  }

  return cache;
}

void SLRUv0_free(cache_t *cache) {
  SLRUv0_params_t *SLRUv0_params = (SLRUv0_params_t *)(cache->eviction_params);
  int i;
  for (i = 0; i < SLRUv0_params->n_seg; i++) LRU_free(SLRUv0_params->LRUs[i]);
  free(SLRUv0_params->LRUs);
  cache_struct_free(cache);
}

/**
 * @brief move an object from ith LRU into (i-1)th LRU, cool
 * (i-1)th LRU if it is full
 *
 * @param cache
 * @param i
 */
void SLRUv0_cool(cache_t *cache, int i) {
  cache_obj_t evicted_obj;
  SLRUv0_params_t *SLRUv0_params = (SLRUv0_params_t *)(cache->eviction_params);
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }
  LRU_evict(SLRUv0_params->LRUs[i], NULL, &evicted_obj);

  if (i == 0) return;

  // If lower LRUs are full
  while (SLRUv0_params->LRUs[i - 1]->occupied_size + evicted_obj.obj_size +
             cache->obj_md_size >
         SLRUv0_params->LRUs[i - 1]->cache_size)
    SLRUv0_cool(cache, i - 1);

  copy_cache_obj_to_request(req_local, &evicted_obj);
  LRU_insert(SLRUv0_params->LRUs[i - 1], req_local);
}

bool SLRUv0_check(cache_t *cache, const request_t *req,
                  const bool update_cache) {
  SLRUv0_params_t *SLRUv0_params = (SLRUv0_params_t *)(cache->eviction_params);
  static __thread request_t *req_local = NULL;
  if (req_local == NULL) {
    req_local = new_request();
  }

  for (int i = 0; i < SLRUv0_params->n_seg; i++) {
    bool cache_hit = LRU_check(SLRUv0_params->LRUs[i], req, false);

    if (cache_hit) {
      // bump object from lower segment to upper segment;
      int src_id = i;
      int dest_id = i + 1;
      if (i == SLRUv0_params->n_seg - 1) {
        dest_id = i;
      }

      LRU_remove(SLRUv0_params->LRUs[src_id], req->obj_id);

      // If the upper LRU is full;
      while (SLRUv0_params->LRUs[dest_id]->occupied_size + req->obj_size +
                 cache->obj_md_size >
             SLRUv0_params->LRUs[dest_id]->cache_size)
        SLRUv0_cool(cache, dest_id);

      LRU_insert(SLRUv0_params->LRUs[dest_id], req);

      return true;
    }
  }
  return false;
}

bool SLRUv0_get(cache_t *cache, const request_t *req) {
  /* because this field cannot be updated in time since segment LRUs are
   * updated, so we should not use this field */
  DEBUG_ASSERT(cache->occupied_size == 0);

  bool ck = cache_get_base(cache, req);

  // SLRUv0_print_cache(cache);
  
  return ck;
}

cache_obj_t *SLRUv0_insert(cache_t *cache, const request_t *req) {
  SLRUv0_params_t *SLRUv0_params = (SLRUv0_params_t *)(cache->eviction_params);

  int i;
  cache_obj_t *cache_obj = NULL;

  // this is used by eviction age tracking
  SLRUv0_params->LRUs[0]->n_req = cache->n_req;

  // Find the lowest LRU with space for insertion
  for (i = 0; i < SLRUv0_params->n_seg; i++) {
    if (SLRUv0_params->LRUs[i]->occupied_size + req->obj_size +
            cache->obj_md_size <=
        SLRUv0_params->LRUs[i]->cache_size) {
      cache_obj = LRU_insert(SLRUv0_params->LRUs[i], req);
      break;
    }
  }

  // If all LRUs are filled, evict an obj from the lowest LRU.
  if (i == SLRUv0_params->n_seg) {
    while (SLRUv0_params->LRUs[0]->occupied_size + req->obj_size +
               cache->obj_md_size >
           SLRUv0_params->LRUs[0]->cache_size) {
      SLRUv0_evict(cache, req, NULL);
    }
    cache_obj = LRU_insert(SLRUv0_params->LRUs[0], req);
  }

  return cache_obj;
}

cache_obj_t *SLRUv0_to_evict(cache_t *cache) {
  SLRUv0_params_t *SLRUv0_params = (SLRUv0_params_t *)(cache->eviction_params);
  for (int i = 0; i < SLRUv0_params->n_seg; i++) {
    if (SLRUv0_params->LRUs[i]->occupied_size > 0) {
      return LRU_to_evict(SLRUv0_params->LRUs[i]);
    }
  }
}

void SLRUv0_evict(cache_t *cache, const request_t *req,
                  cache_obj_t *evicted_obj) {
  SLRUv0_params_t *SLRUv0_params = (SLRUv0_params_t *)(cache->eviction_params);

  int nth_seg_to_evict = 0;
  for (int i = 0; i < SLRUv0_params->n_seg; i++) {
    if (SLRUv0_params->LRUs[i]->occupied_size > 0) {
      nth_seg_to_evict = i;
      break;
    }
  }

#ifdef TRACK_EVICTION_R_AGE
  record_eviction_age(
      cache, req->real_time -
                 SLRUv0_params->LRUs[nth_seg_to_evict]->q_tail->create_time);
#endif
#ifdef TRACK_EVICTION_V_AGE
  record_eviction_age(
      cache, cache->n_req -
                 SLRUv0_params->LRUs[nth_seg_to_evict]->q_tail->create_time);
#endif

  cache_evict_LRU(SLRUv0_params->LRUs[nth_seg_to_evict], req, evicted_obj);
}

bool SLRUv0_remove(cache_t *cache, const obj_id_t obj_id) {
  SLRUv0_params_t *SLRUv0_params = (SLRUv0_params_t *)(cache->eviction_params);
  cache_obj_t *obj;
  for (int i = 0; i < SLRUv0_params->n_seg; i++) {
    obj = cache_get_obj_by_id(SLRUv0_params->LRUs[i], obj_id);
    if (obj) {
      remove_obj_from_list(&(SLRUv0_params->LRUs[i])->q_head,
                           &(SLRUv0_params->LRUs[i])->q_tail, obj);
      cache_remove_obj_base(SLRUv0_params->LRUs[i], obj);
      return true;
    }
  }
  if (obj == NULL) {
    return false;
  }
}

#ifdef __cplusplus
extern "C"
}
#endif
