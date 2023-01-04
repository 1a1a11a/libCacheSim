//
//  segmented LRU implemented using multiple lists instead of multiple LRUs
//  this has a better performance than SLRUv0, but it is very hard to implement
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

// ############################## debug functions ##############################
#define DEBUG_MODE
#undef DEBUG_MODE
#ifdef DEBUG_MODE
#define DEBUG_PRINT_CACHE_STATE(cache, params, req)                            \
  do {                                                                         \
    printf("%ld %ld %s: ", cache->n_req, req->obj_id, __func__);               \
    for (int i = 0; i < params->n_seg; i++) {                                  \
      printf("%ld/%ld/%p/%p, ", params->lru_n_objs[i], params->lru_n_bytes[i], \
             params->lru_heads[i], params->lru_tails[i]);                      \
    }                                                                          \
    printf("\n");                                                              \
    _SLRU_verify_lru_size(cache);                                              \
  } while (0)

#define DEBUG_PRINT_CACHE(cache, params)                 \
  do {                                                   \
    for (int i = params->n_seg - 1; i >= 0; i--) {       \
      cache_obj_t *obj = params->lru_heads[i];           \
      while (obj != NULL) {                              \
        printf("%lu(%u)->", obj->obj_id, obj->obj_size); \
        obj = obj->queue.next;                           \
      }                                                  \
      printf(" | ");                                     \
    }                                                    \
    printf("\n");                                        \
  } while (0)

#else
#define DEBUG_PRINT_CACHE_STATE(cache, params, req)
#define DEBUG_PRINT_CACHE(cache, params)
#endif

#undef DEBUG_PRINT_CACHE_STATE
#define DEBUG_PRINT_CACHE_STATE(cache, params, req)

static void _SLRU_verify_lru_size(cache_t *cache) {
  SLRU_params_t *params = (SLRU_params_t *)cache->eviction_params;
  for (int i = 0; i < params->n_seg; i++) {
    int64_t n_objs = 0;
    int64_t n_bytes = 0;
    cache_obj_t *obj = params->lru_heads[i];
    while (obj != NULL) {
      n_objs += 1;
      n_bytes += obj->obj_size;
      obj = obj->queue.next;
    }
    assert(n_objs == params->lru_n_objs[i]);
    assert(n_bytes == params->lru_n_bytes[i]);
  }
}

bool SLRU_get_debug(cache_t *cache, const request_t *req) {
  cache->n_req += 1;

  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  bool cache_hit = cache->check(cache, req, true);

  if (cache_hit) {
    DEBUG_PRINT_CACHE(cache, params);
    return cache_hit;
  }

  if (cache->can_insert(cache, req) == false) {
    DEBUG_PRINT_CACHE(cache, params);
    return cache_hit;
  }

  if (!cache_hit) {
    while (cache->occupied_size + req->obj_size + cache->obj_md_size >
           cache->cache_size) {
      cache->evict(cache, req, NULL);
    }

    cache->insert(cache, req);
  }

  DEBUG_PRINT_CACHE(cache, params);

  return cache_hit;
}

// ######################## internal function ###########################
/* SLRU cannot insert an object larger than segment size */
bool SLRU_can_insert(cache_t *cache, const request_t *req) {
  SLRU_params_t *params = (SLRU_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);
  return can_insert &&
         (req->obj_size + cache->obj_md_size <= params->per_seg_max_size);
}

/**
 * @brief move an object from ith LRU into (i-1)th LRU, cool
 * (i-1)th LRU if it is full, where the n_seg th LRU is the most recent
 *
 * @param cache
 * @param i
 */
static void SLRU_cool(cache_t *cache, const request_t *req, const int id) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  if (id == 0) return SLRU_evict(cache, req, NULL);

  cache_obj_t *obj = params->lru_tails[id];
  DEBUG_ASSERT(obj != NULL);
  DEBUG_ASSERT(obj->SLRU.lru_id == id);
  remove_obj_from_list(&params->lru_heads[id], &params->lru_tails[id], obj);
  prepend_obj_to_head(&params->lru_heads[id - 1], &params->lru_tails[id - 1],
                      obj);
  obj->SLRU.lru_id = id - 1;
  params->lru_n_bytes[id] -= obj->obj_size;
  params->lru_n_bytes[id - 1] += obj->obj_size;
  params->lru_n_objs[id]--;
  params->lru_n_objs[id - 1]++;

  // If lower LRUs are full
  while (params->lru_n_bytes[id - 1] > params->per_seg_max_size) {
    SLRU_cool(cache, req, id - 1);
  }
}

/**
 * @brief promote the object from the current segment to the next (i+1) segment
 */
static void SLRU_promote_to_next_seg(cache_t *cache, const request_t *req,
                                     cache_obj_t *obj) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  int id = obj->SLRU.lru_id;
  remove_obj_from_list(&params->lru_heads[id], &params->lru_tails[id], obj);
  params->lru_n_bytes[id] -= obj->obj_size + cache->obj_md_size;
  params->lru_n_objs[id]--;

  obj->SLRU.lru_id += 1;
  prepend_obj_to_head(&params->lru_heads[id + 1], &params->lru_tails[id + 1],
                      obj);
  params->lru_n_bytes[id + 1] += obj->obj_size + cache->obj_md_size;
  params->lru_n_objs[id + 1]++;
}

// ######################## external function ###########################
bool SLRU_get(cache_t *cache, const request_t *req) {
#ifdef DEBUG_MODE
  return SLRU_get_debug(cache, req);
#else
  bool ck = cache_get_base(cache, req);
#endif
}

/**
 * @brief check whether an object is in the cache,
 * promote to the next segment if update_cache is true
 */
bool SLRU_check(cache_t *cache, const request_t *req, const bool update_cache) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  cache_obj_t *obj = cache_get_obj(cache, req);

  if (obj == NULL) {
    return false;
  }

  if (!update_cache) {
    return true;
  }

  if (obj->SLRU.lru_id == params->n_seg - 1) {
    move_obj_to_head(&params->lru_heads[params->n_seg - 1],
                     &params->lru_tails[params->n_seg - 1], obj);
  } else {
    SLRU_promote_to_next_seg(cache, req, obj);

    while (params->lru_n_bytes[obj->SLRU.lru_id] > params->per_seg_max_size) {
      // if the LRU is full
      SLRU_cool(cache, req, obj->SLRU.lru_id);
    }
    DEBUG_ASSERT(cache->occupied_size <= cache->cache_size);
  }

  return true;
}

cache_obj_t *SLRU_insert(cache_t *cache, const request_t *req) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  cache_obj_t *obj = cache_insert_base(cache, req);

  // Find the lowest LRU with space for insertion
  int nth_seg = -1;
  for (int i = 0; i < params->n_seg; i++) {
    if (params->lru_n_bytes[i] + req->obj_size + cache->obj_md_size <=
        params->per_seg_max_size) {
      nth_seg = i;
      break;
    }
  }

  if (nth_seg == -1) {
    // No space for insertion
    while (cache->occupied_size + req->obj_size + cache->obj_md_size >
           cache->cache_size) {
      cache->evict(cache, req, NULL);
    }
    nth_seg = 0;
  }

  prepend_obj_to_head(&params->lru_heads[nth_seg], &params->lru_tails[nth_seg],
                      obj);
  obj->SLRU.lru_id = nth_seg;
  params->lru_n_bytes[nth_seg] += req->obj_size + cache->obj_md_size;
  params->lru_n_objs[nth_seg]++;

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
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  int nth_seg = 0;
  for (int i = 0; i < params->n_seg; i++) {
    if (params->lru_n_bytes[i] > 0) {
      nth_seg = i;
      break;
    }
  }

  cache_obj_t *obj = params->lru_tails[nth_seg];
  DEBUG_ASSERT(obj != NULL);

  if (evicted_obj != NULL) {
    memcpy(evicted_obj, obj, sizeof(cache_obj_t));
  }

  params->lru_n_bytes[nth_seg] -= obj->obj_size + cache->obj_md_size;
  params->lru_n_objs[nth_seg]--;

  remove_obj_from_list(&params->lru_heads[nth_seg], &params->lru_tails[nth_seg],
                       obj);
  cache_evict_base(cache, obj, true);
}

bool SLRU_remove(cache_t *cache, const obj_id_t obj_id) {
  SLRU_params_t *params = (SLRU_params_t *)(cache->eviction_params);
  cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);

  if (obj == NULL) {
    return false;
  }

  remove_obj_from_list(&(params->lru_heads[obj->SLRU.lru_id]),
                       &(params->lru_tails[obj->SLRU.lru_id]), obj);

  cache_remove_obj_base(cache, obj, true);

  return true;
}

// ######################## setup function ###########################
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
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->can_insert = SLRU_can_insert;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
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