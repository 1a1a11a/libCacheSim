//
//  segmented LRU
//
//  SLRU.c
//  libCacheSim
//
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/SLRU.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SLRU_params {
  cache_obj_t **lru_heads;
  cache_obj_t **lru_tails;
  int64_t *lru_n_bytes;
  int64_t *lru_n_objs;
  int64_t per_seg_max_size;
  int n_seg;
} SLRU_params_t;

static const char *SLRU_current_params(SLRU_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-seg=%d\n", params->n_seg);
  return params_str;
}

static void SLRU_parse_params(cache_t *cache,
                               const char *cache_specific_params) {
  SLRU_params_t *params = (SLRU_params_t *)cache->eviction_params;
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
      printf("current parameters: %s\n", SLRU_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }
  free(old_params_str);
}

/* SLRU cannot insert an object larger than segment size */
bool SLRU_can_insert(cache_t *cache, const request_t *req) {
  SLRU_params_t *params = (SLRU_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);
  return can_insert && (req->obj_size + cache->per_obj_metadata_size <=
                        params->per_seg_max_size);
}

/**
 * @brief move an object from ith LRU into (i-1)th LRU, cool
 * (i-1)th LRU if it is full, where the n_seg th LRU is the most recent
 *
 * @param cache
 * @param i
 */
static void SLRU_cool(cache_t *cache, const request_t *req, const int i) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);

  if (i == 0) return SLRU_evict(cache, req, NULL);

  cache_obj_t *obj = params->lru_tails[i];
  DEBUG_ASSERT(obj != NULL);
  DEBUG_ASSERT(obj->SLRU.lru_id == i);
  remove_obj_from_list(&params->lru_heads[i], &params->lru_tails[i], obj);
  prepend_obj_to_head(&params->lru_heads[i - 1], &params->lru_tails[i - 1],
                      obj);
  obj->SLRU.lru_id = i - 1;
  params->lru_n_bytes[i] -= obj->obj_size;
  params->lru_n_bytes[i - 1] += obj->obj_size;
  params->lru_n_objs[i]--;
  params->lru_n_objs[i - 1]++;

  // If lower LRUs are full
  while (params->lru_n_bytes[i - 1] > params->per_seg_max_size) {
    SLRU_cool(cache, req, i - 1);
  }
}

/**
 * @brief promote the object from the current segment to the next (i+1) segment
 */
static void SLRU_promote_to_next_seg(cache_t *cache, const request_t *req,
                                      cache_obj_t *obj) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);

  int seg_id = obj->SLRU.lru_id;
  remove_obj_from_list(&params->lru_heads[seg_id], &params->lru_tails[seg_id],
                       obj);
  prepend_obj_to_head(&params->lru_heads[seg_id + 1],
                      &params->lru_tails[seg_id + 1], obj);
  obj->SLRU.lru_id += 1;
  params->lru_n_bytes[seg_id] -= obj->obj_size + cache->per_obj_metadata_size;
  params->lru_n_bytes[seg_id + 1] +=
      obj->obj_size + cache->per_obj_metadata_size;
  params->lru_n_objs[seg_id]--;
  params->lru_n_objs[seg_id + 1]++;

  while (params->lru_n_bytes[seg_id + 1] > params->per_seg_max_size) {
    SLRU_cool(cache, req, seg_id + 1);
  }
}

static void _SLRU_print_cache(cache_t *cache) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  for (int i = 0; i < params->n_seg; i++) {
    cache_obj_t *obj = params->lru_heads[i];
    while (obj != NULL) {
      printf("%lu->", obj->obj_id);
      obj = obj->queue.next;
    }
    printf(" | ");
  }
  printf("\n");
}

cache_ck_res_e SLRU_get(cache_t *cache, const request_t *req) {
  cache_ck_res_e ck = cache_get_base(cache, req);

  // _SLRU_print_cache(cache);

  return ck;
}

/**
 * @brief check whether an object is in the cache,
 * promote to the next segment if update_cache is true
 */
cache_ck_res_e SLRU_check(cache_t *cache, const request_t *req,
                           const bool update_cache) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_get_obj(cache, req);

  if (obj == NULL) {
    return cache_ck_miss;
  }

  if (obj->SLRU.lru_id == params->n_seg - 1) {
    move_obj_to_head(&params->lru_heads[params->n_seg - 1],
                     &params->lru_tails[params->n_seg - 1], obj);
  } else {
    SLRU_promote_to_next_seg(cache, req, obj);
  }

  int64_t size_change = req->obj_size - obj->obj_size;
  cache->occupied_size += size_change;
  params->lru_n_bytes[obj->SLRU.lru_id] += size_change;

  return cache_ck_hit;
}

cache_obj_t *SLRU_insert(cache_t *cache, const request_t *req) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);

  cache_obj_t *obj = hashtable_insert(cache->hashtable, req);

  // Find the lowest LRU with space for insertion
  for (int i = 0; i < params->n_seg; i++) {
    if (params->lru_n_bytes[i] + req->obj_size + cache->per_obj_metadata_size <=
        params->per_seg_max_size) {
      prepend_obj_to_head(&params->lru_heads[i], &params->lru_tails[i], obj);
      obj->SLRU.lru_id = i;
      params->lru_n_bytes[i] += req->obj_size + cache->per_obj_metadata_size;
      params->lru_n_objs[i]++;

      break;
    }
  }
  cache->n_obj += 1;
  cache->occupied_size += req->obj_size + cache->per_obj_metadata_size;

  return obj;
}

cache_obj_t *SLRU_to_evict(cache_t *cache) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  for (int i = 0; i < params->n_seg; i++) {
    if (params->lru_n_bytes[i] > 0) {
      return params->lru_tails[i];
    }
  }
}

void SLRU_evict(cache_t *cache, const request_t *req,
                 cache_obj_t *evicted_obj) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);

  int nth_seg = 0;
  for (int i = 0; i < params->n_seg; i++) {
    if (params->lru_n_bytes[i] > 0) {
      nth_seg = i;
      break;
    }
  }

  cache_obj_t *obj = params->lru_tails[nth_seg];
#ifdef TRACK_EVICTION_R_AGE
  record_eviction_age(cache, req->real_time - obj->create_rtime);
#endif
#ifdef TRACK_EVICTION_V_AGE
  record_eviction_age(cache, cache->n_req - obj->create_vtime);
#endif

  cache->n_obj -= 1;
  cache->occupied_size -= obj->obj_size + cache->per_obj_metadata_size;
  params->lru_n_bytes[nth_seg] -= obj->obj_size + cache->per_obj_metadata_size;
  params->lru_n_objs[nth_seg]--;

  if (evicted_obj != NULL) {
    memcpy(evicted_obj, obj, sizeof(cache_obj_t));
  }

  DEBUG_ASSERT(obj != NULL);
  remove_obj_from_list(&params->lru_heads[nth_seg], &params->lru_tails[nth_seg],
                       obj);
  hashtable_delete(cache->hashtable, obj);
}

bool SLRU_remove(cache_t *cache, const obj_id_t obj_id) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);

  if (obj == NULL) {
    return false;
  }

  cache->occupied_size -= (obj->obj_size + cache->per_obj_metadata_size);
  cache->n_obj -= 1;
  remove_obj_from_list(&(params->lru_heads[obj->SLRU.lru_id]),
                       &(params->lru_tails[obj->SLRU.lru_id]), obj);
  hashtable_delete(cache->hashtable, obj);

  return true;
}

void SLRU_free(cache_t *cache) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  free(params->lru_heads);
  free(params->lru_tails);
  free(params->lru_n_objs);
  free(params->lru_n_bytes);
  cache_struct_free(cache);
}

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
  // cache->can_insert = cache_can_insert_default;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->can_insert = SLRU_can_insert;
  // cache->get_occupied_byte = SLRU_get_occupied_byte;
  // cache->get_n_obj = SLRU_get_n_obj;

  if (ccache_params.consider_obj_metadata) {
    cache->per_obj_metadata_size = 8 * 2;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  cache->eviction_params = (SLRU_params_t *)malloc(sizeof(SLRU_params_t));
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  params->n_seg = 4;

  if (cache_specific_params != NULL) {
    SLRU_parse_params(cache, cache_specific_params);
  }

  params->per_seg_max_size = ccache_params.cache_size / params->n_seg;
  params->lru_heads =
      (cache_obj_t **)malloc(sizeof(cache_obj_t *) * params->n_seg);
  params->lru_tails =
      (cache_obj_t **)malloc(sizeof(cache_obj_t *) * params->n_seg);
  params->lru_n_objs = (int64_t *)malloc(sizeof(int64_t) * params->n_seg);
  params->lru_n_bytes = (int64_t *)malloc(sizeof(int64_t) * params->n_seg);

  for (int i = 0; i < params->n_seg; i++) {
    params->lru_heads[i] = NULL;
    params->lru_tails[i] = NULL;
    params->lru_n_objs[i] = 0;
    params->lru_n_bytes[i] = 0;
  }

  return cache;
}

#ifdef __cplusplus
extern "C"
}
#endif