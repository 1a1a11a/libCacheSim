//
//  segmented fifo implemented using multiple lists instead of multiple fifos
//  this has a better performance than SFIFOv0, but it is very hard to implement
//
//  SFIFO.c
//  libCacheSim
//
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/priv/SFIFO.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SFIFO_params {
  cache_obj_t **fifo_heads;
  cache_obj_t **fifo_tails;
  int64_t *fifo_n_bytes;
  int64_t *fifo_n_objs;
  int64_t per_seg_max_size;
  int n_seg;
} SFIFO_params_t;

// ############################## debug functions ##############################
#define DEBUG_MODE
#undef DEBUG_MODE
#ifdef DEBUG_MODE
#define DEBUG_PRINT_CACHE_STATE(cache, params, req)                            \
  do {                                                                         \
    printf("%ld %ld %s: ", cache->n_req, req->obj_id, __func__);               \
    for (int i = 0; i < params->n_seg; i++) {                                  \
      printf("%ld/%ld/%p/%p, ", params->fifo_n_objs[i], params->fifo_n_bytes[i], \
             params->fifo_heads[i], params->fifo_tails[i]);                      \
    }                                                                          \
    printf("\n");                                                              \
    _SFIFO_verify_fifo_size(cache);                                              \
  } while (0)

#define DEBUG_PRINT_CACHE(cache, params)                 \
  do {                                                   \
    for (int i = params->n_seg - 1; i >= 0; i--) {       \
      cache_obj_t *obj = params->fifo_heads[i];           \
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

static void _SFIFO_verify_fifo_size(cache_t *cache) {
  SFIFO_params_t *params = (SFIFO_params_t *)cache->eviction_params;
  for (int i = 0; i < params->n_seg; i++) {
    int64_t n_objs = 0;
    int64_t n_bytes = 0;
    cache_obj_t *obj = params->fifo_heads[i];
    while (obj != NULL) {
      n_objs += 1;
      n_bytes += obj->obj_size;
      obj = obj->queue.next;
    }
    assert(n_objs == params->fifo_n_objs[i]);
    assert(n_bytes == params->fifo_n_bytes[i]);
  }
}

bool SFIFO_get_debug(cache_t *cache, const request_t *req) {
  cache->n_req += 1;

  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
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
/* SFIFO cannot insert an object larger than segment size */
bool SFIFO_can_insert(cache_t *cache, const request_t *req) {
  SFIFO_params_t *params = (SFIFO_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);
  return can_insert &&
         (req->obj_size + cache->obj_md_size <= params->per_seg_max_size);
}

/**
 * @brief move an object from ith fifo into (i-1)th fifo, cool
 * (i-1)th fifo if it is full, where the n_seg th fifo is the most recent
 *
 * @param cache
 * @param i
 */
static void SFIFO_cool(cache_t *cache, const request_t *req, const int id) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  if (id == 0) return SFIFO_evict(cache, req, NULL);

  cache_obj_t *obj = params->fifo_tails[id];
  DEBUG_ASSERT(obj != NULL);
  DEBUG_ASSERT(obj->SFIFO.fifo_id == id);
  remove_obj_from_list(&params->fifo_heads[id], &params->fifo_tails[id], obj);
  prepend_obj_to_head(&params->fifo_heads[id - 1], &params->fifo_tails[id - 1],
                      obj);
  obj->SFIFO.fifo_id = id - 1;
  params->fifo_n_bytes[id] -= obj->obj_size;
  params->fifo_n_bytes[id - 1] += obj->obj_size;
  params->fifo_n_objs[id]--;
  params->fifo_n_objs[id - 1]++;

  // If lower fifos are full
  while (params->fifo_n_bytes[id - 1] > params->per_seg_max_size) {
    SFIFO_cool(cache, req, id - 1);
  }
}

/**
 * @brief promote the object from the current segment to the next (i+1) segment
 */
static void SFIFO_promote_to_next_seg(cache_t *cache, const request_t *req,
                                     cache_obj_t *obj) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  int id = obj->SFIFO.fifo_id;
  remove_obj_from_list(&params->fifo_heads[id], &params->fifo_tails[id], obj);
  params->fifo_n_bytes[id] -= obj->obj_size + cache->obj_md_size;
  params->fifo_n_objs[id]--;

  obj->SFIFO.fifo_id += 1;
  prepend_obj_to_head(&params->fifo_heads[id + 1], &params->fifo_tails[id + 1],
                      obj);
  params->fifo_n_bytes[id + 1] += obj->obj_size + cache->obj_md_size;
  params->fifo_n_objs[id + 1]++;
}

// ######################## external function ###########################
bool SFIFO_get(cache_t *cache, const request_t *req) {
#ifdef DEBUG_MODE
  return SFIFO_get_debug(cache, req);
#else
  bool ck = cache_get_base(cache, req);
#endif
}

/**
 * @brief check whether an object is in the cache,
 * promote to the next segment if update_cache is true
 */
bool SFIFO_check(cache_t *cache, const request_t *req, const bool update_cache) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  cache_obj_t *obj = cache_get_obj(cache, req);

  if (obj == NULL) {
    return false;
  }

  if (!update_cache) {
    return true;
  }

  if (obj->SFIFO.fifo_id != params->n_seg - 1) {
    SFIFO_promote_to_next_seg(cache, req, obj);

    while (params->fifo_n_bytes[obj->SFIFO.fifo_id] > params->per_seg_max_size) {
      // if the fifo is full
      SFIFO_cool(cache, req, obj->SFIFO.fifo_id);
    }
    DEBUG_ASSERT(cache->occupied_size <= cache->cache_size);
  }

  return true;
}

cache_obj_t *SFIFO_insert(cache_t *cache, const request_t *req) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  cache_obj_t *obj = hashtable_insert(cache->hashtable, req);

  // Find the lowest fifo with space for insertion
  int nth_seg = -1;
  for (int i = 0; i < params->n_seg; i++) {
    if (params->fifo_n_bytes[i] + req->obj_size + cache->obj_md_size <=
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

  prepend_obj_to_head(&params->fifo_heads[nth_seg], &params->fifo_tails[nth_seg],
                      obj);
  obj->SFIFO.fifo_id = nth_seg;
  params->fifo_n_bytes[nth_seg] += req->obj_size + cache->obj_md_size;
  params->fifo_n_objs[nth_seg]++;
  cache->n_obj += 1;
  cache->occupied_size += req->obj_size + cache->obj_md_size;

  return obj;
}

cache_obj_t *SFIFO_to_evict(cache_t *cache) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  for (int i = 0; i < params->n_seg; i++) {
    if (params->fifo_n_bytes[i] > 0) {
      return params->fifo_tails[i];
    }
  }
}

void SFIFO_evict(cache_t *cache, const request_t *req,
                cache_obj_t *evicted_obj) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  DEBUG_PRINT_CACHE_STATE(cache, params, req);

  int nth_seg = 0;
  for (int i = 0; i < params->n_seg; i++) {
    if (params->fifo_n_bytes[i] > 0) {
      nth_seg = i;
      break;
    }
  }

  cache_obj_t *obj = params->fifo_tails[nth_seg];

#ifdef TRACK_EVICTION_R_AGE
  record_eviction_age(cache, req->real_time - obj->create_time);
#endif
#ifdef TRACK_EVICTION_V_AGE
  record_eviction_age(cache, cache->n_req - obj->create_time);
#endif

  cache->n_obj -= 1;
  cache->occupied_size -= obj->obj_size + cache->obj_md_size;
  params->fifo_n_bytes[nth_seg] -= obj->obj_size + cache->obj_md_size;
  params->fifo_n_objs[nth_seg]--;

  if (evicted_obj != NULL) {
    memcpy(evicted_obj, obj, sizeof(cache_obj_t));
  }

  DEBUG_ASSERT(obj != NULL);
  remove_obj_from_list(&params->fifo_heads[nth_seg], &params->fifo_tails[nth_seg],
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
  remove_obj_from_list(&(params->fifo_heads[obj->SFIFO.fifo_id]),
                       &(params->fifo_tails[obj->SFIFO.fifo_id]), obj);
  hashtable_delete(cache->hashtable, obj);

  return true;
}

// ######################## setup function ###########################
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

void SFIFO_free(cache_t *cache) {
  SFIFO_params_t *params = (SFIFO_params_t *)(cache->eviction_params);
  free(params->fifo_heads);
  free(params->fifo_tails);
  free(params->fifo_n_objs);
  free(params->fifo_n_bytes);
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
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->get_n_obj = cache_get_n_obj_default;
  cache->can_insert = SFIFO_can_insert;

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
  params->fifo_heads =
      (cache_obj_t **)malloc(sizeof(cache_obj_t *) * params->n_seg);
  params->fifo_tails =
      (cache_obj_t **)malloc(sizeof(cache_obj_t *) * params->n_seg);
  params->fifo_n_objs = (int64_t *)malloc(sizeof(int64_t) * params->n_seg);
  params->fifo_n_bytes = (int64_t *)malloc(sizeof(int64_t) * params->n_seg);

  for (int i = 0; i < params->n_seg; i++) {
    params->fifo_heads[i] = NULL;
    params->fifo_tails[i] = NULL;
    params->fifo_n_objs[i] = 0;
    params->fifo_n_bytes[i] = 0;
  }

  return cache;
}

#ifdef __cplusplus
extern "C"
}
#endif