//
//  segmented LRU
//
//  SFIFO.c
//  libCacheSim
//
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/SFIFO.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SFIFO_params {
  cache_obj_t **lru_heads;
  cache_obj_t **lru_tails;
  int64_t *lru_n_bytes;
  int64_t *lru_n_objs;
  int64_t per_seg_max_size;
  int n_seg;
} SFIFO_params_t;

static const char *SFIFO_current_params(SFIFO_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-seg=%d\n", params->n_seg);
  return params_str;
}

static void SFIFO_parse_params(cache_t *cache,
                               const char *cache_specific_params) {
  SFIFO_params_t *params = (SFIFO_params_t *)cache->eviction_params;
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
      printf("current parameters: %s\n", SFIFO_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }
  free(old_params_str);
}

/* SFIFO cannot insert an object larger than segment size */
bool SFIFO_can_insert(cache_t *cache, const request_t *req) {
  SFIFO_params_t *params = (SFIFO_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);
  return can_insert && (req->obj_size + cache->obj_md_size <=
                        params->per_seg_max_size);
}

/**
 * @brief move an object from ith LRU into (i-1)th LRU, cool
 * (i-1)th LRU if it is full, where the n_seg th LRU is the most recent
 *
 * @param cache
 * @param i
 */
static void SFIFO_cool(cache_t *cache, const request_t *req, const int i) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);

  if (i == 0) return SFIFO_evict(cache, req, NULL);

  cache_obj_t *obj = params->lru_tails[i];
  DEBUG_ASSERT(obj != NULL);
  DEBUG_ASSERT(obj->SFIFO.lru_id == i);
  remove_obj_from_list(&params->lru_heads[i], &params->lru_tails[i], obj);
  prepend_obj_to_head(&params->lru_heads[i - 1], &params->lru_tails[i - 1],
                      obj);
  obj->SFIFO.lru_id = i - 1;
  params->lru_n_bytes[i] -= obj->obj_size;
  params->lru_n_bytes[i - 1] += obj->obj_size;
  params->lru_n_objs[i]--;
  params->lru_n_objs[i - 1]++;

  // If lower LRUs are full
  while (params->lru_n_bytes[i - 1] > params->per_seg_max_size) {
    SFIFO_cool(cache, req, i - 1);
  }
}

/**
 * @brief promote the object from the current segment to the next (i+1) segment
 */
static void SFIFO_promote_to_next_seg(cache_t *cache, const request_t *req,
                                      cache_obj_t *obj) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);

  int seg_id = obj->SFIFO.lru_id;
  remove_obj_from_list(&params->lru_heads[seg_id], &params->lru_tails[seg_id],
                       obj);
  prepend_obj_to_head(&params->lru_heads[seg_id + 1],
                      &params->lru_tails[seg_id + 1], obj);
  obj->SFIFO.lru_id += 1;
  params->lru_n_bytes[seg_id] -= obj->obj_size + cache->obj_md_size;
  params->lru_n_bytes[seg_id + 1] +=
      obj->obj_size + cache->obj_md_size;
  params->lru_n_objs[seg_id]--;
  params->lru_n_objs[seg_id + 1]++;

  while (params->lru_n_bytes[seg_id + 1] > params->per_seg_max_size) {
    SFIFO_cool(cache, req, seg_id + 1);
  }
}

static void _SFIFO_print_cache(cache_t *cache) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
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

cache_ck_res_e SFIFO_get(cache_t *cache, const request_t *req) {
  cache_ck_res_e ck = cache_get_base(cache, req);

  // _SFIFO_print_cache(cache);

  return ck;
}

/**
 * @brief check whether an object is in the cache,
 * promote to the next segment if update_cache is true
 */
cache_ck_res_e SFIFO_check(cache_t *cache, const request_t *req,
                           const bool update_cache) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_get_obj(cache, req);

  if (obj == NULL) {
    return cache_ck_miss;
  }

  if (obj->SFIFO.lru_id == params->n_seg - 1) {
    move_obj_to_head(&params->lru_heads[params->n_seg - 1],
                     &params->lru_tails[params->n_seg - 1], obj);
  } else {
    SFIFO_promote_to_next_seg(cache, req, obj);
  }

  int64_t size_change = req->obj_size - obj->obj_size;
  cache->occupied_size += size_change;
  params->lru_n_bytes[obj->SFIFO.lru_id] += size_change;

  return cache_ck_hit;
}

cache_obj_t *SFIFO_insert(cache_t *cache, const request_t *req) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);

  cache_obj_t *obj = hashtable_insert(cache->hashtable, req);

  // Find the lowest LRU with space for insertion
  for (int i = 0; i < params->n_seg; i++) {
    if (params->lru_n_bytes[i] + req->obj_size + cache->obj_md_size <=
        params->per_seg_max_size) {
      prepend_obj_to_head(&params->lru_heads[i], &params->lru_tails[i], obj);
      obj->SFIFO.lru_id = i;
      params->lru_n_bytes[i] += req->obj_size + cache->obj_md_size;
      params->lru_n_objs[i]++;

      break;
    }
  }
  cache->n_obj += 1;
  cache->occupied_size += req->obj_size + cache->obj_md_size;

  return obj;
}

cache_obj_t *SFIFO_to_evict(cache_t *cache) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  for (int i = 0; i < params->n_seg; i++) {
    if (params->lru_n_bytes[i] > 0) {
      return params->lru_tails[i];
    }
  }
}

void SFIFO_evict(cache_t *cache, const request_t *req,
                 cache_obj_t *evicted_obj) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);

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
  cache->occupied_size -= obj->obj_size + cache->obj_md_size;
  params->lru_n_bytes[nth_seg] -= obj->obj_size + cache->obj_md_size;
  params->lru_n_objs[nth_seg]--;

  if (evicted_obj != NULL) {
    memcpy(evicted_obj, obj, sizeof(cache_obj_t));
  }

  DEBUG_ASSERT(obj != NULL);
  remove_obj_from_list(&params->lru_heads[nth_seg], &params->lru_tails[nth_seg],
                       obj);
  hashtable_delete(cache->hashtable, obj);
}

bool SFIFO_remove(cache_t *cache, const obj_id_t obj_id) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);

  if (obj == NULL) {
    return false;
  }

  cache->occupied_size -= (obj->obj_size + cache->obj_md_size);
  cache->n_obj -= 1;
  remove_obj_from_list(&(params->lru_heads[obj->SFIFO.lru_id]),
                       &(params->lru_tails[obj->SFIFO.lru_id]), obj);
  hashtable_delete(cache->hashtable, obj);

  return true;
}

void SFIFO_free(cache_t *cache) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  free(params->lru_heads);
  free(params->lru_tails);
  free(params->lru_n_objs);
  free(params->lru_n_bytes);
  cache_struct_free(cache);
}

cache_t *SFIFO_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("SFIFO", ccache_params);
  cache->cache_init = SFIFO_init;
  cache->cache_free = SFIFO_free;
  cache->get = SFIFO_get;
  cache->check = SFIFO_check;
  cache->insert = SFIFO_insert;
  cache->evict = SFIFO_evict;
  cache->remove = SFIFO_remove;
  cache->to_evict = SFIFO_to_evict;
  cache->init_params = cache_specific_params;
  // cache->can_insert = cache_can_insert_default;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->can_insert = SFIFO_can_insert;
  // cache->get_occupied_byte = SFIFO_get_occupied_byte;
  // cache->get_n_obj = SFIFO_get_n_obj;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = (SFIFO_params_t *)malloc(sizeof(SFIFO_params_t));
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  params->n_seg = 4;

  if (cache_specific_params != NULL) {
    SFIFO_parse_params(cache, cache_specific_params);
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