//
// Multi-Queue (MQ) cache eviction policy.
// Objects are tracked in multiple LRU queues based on logarithmic frequency.
//

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MQ_MAX_N_QUEUE 32

typedef struct {
  cache_obj_t **q_heads;
  cache_obj_t **q_tails;
  int64_t *q_n_bytes;
  int64_t *q_n_objs;
  int n_queue;
} MQ_params_t;

static const char *DEFAULT_CACHE_PARAMS = "n-queue=8";

static void MQ_free(cache_t *cache);
static bool MQ_get(cache_t *cache, const request_t *req);
static cache_obj_t *MQ_find(cache_t *cache, const request_t *req,
                            bool update_cache);
static cache_obj_t *MQ_insert(cache_t *cache, const request_t *req);
static cache_obj_t *MQ_to_evict(cache_t *cache, const request_t *req);
static void MQ_evict(cache_t *cache, const request_t *req);
static bool MQ_remove(cache_t *cache, obj_id_t obj_id);

static void MQ_remove_obj(cache_t *cache, cache_obj_t *obj);
static void MQ_parse_params(cache_t *cache, const char *cache_specific_params);

static inline int MQ_level(int64_t freq, int n_queue) {
  int level = 0;
  while (freq > 1 && level < n_queue - 1) {
    freq >>= 1;
    level++;
  }
  return level;
}

cache_t *MultiQueue_init(const common_cache_params_t ccache_params,
                         const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("MultiQueue", ccache_params, cache_specific_params);
  cache->cache_init = MultiQueue_init;
  cache->cache_free = MQ_free;
  cache->get = MQ_get;
  cache->find = MQ_find;
  cache->insert = MQ_insert;
  cache->evict = MQ_evict;
  cache->remove = MQ_remove;
  cache->to_evict = MQ_to_evict;
  cache->get_occupied_byte = cache_get_occupied_byte_default;
  cache->can_insert = cache_can_insert_default;
  cache->get_n_obj = cache_get_n_obj_default;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 8 * 2;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = malloc(sizeof(MQ_params_t));
  memset(cache->eviction_params, 0, sizeof(MQ_params_t));
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;

  params->n_queue = 8;
  MQ_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    MQ_parse_params(cache, cache_specific_params);
  }

  params->q_heads = calloc(params->n_queue, sizeof(cache_obj_t *));
  params->q_tails = calloc(params->n_queue, sizeof(cache_obj_t *));
  params->q_n_bytes = calloc(params->n_queue, sizeof(int64_t));
  params->q_n_objs = calloc(params->n_queue, sizeof(int64_t));

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "MQ(%d)", params->n_queue);

  return cache;
}

static void MQ_free(cache_t *cache) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  free(params->q_heads);
  free(params->q_tails);
  free(params->q_n_bytes);
  free(params->q_n_objs);
  free(params);
  cache_struct_free(cache);
}

static bool MQ_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

static cache_obj_t *MQ_find(cache_t *cache, const request_t *req,
                            bool update_cache) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);
  if (!obj || !update_cache) {
    return obj;
  }

  obj->misc.freq += 1;
  int curr_level = obj->SLRU.lru_id;
  int next_level = MQ_level(obj->misc.freq, params->n_queue);

  if (next_level != curr_level) {
    remove_obj_from_list(&params->q_heads[curr_level], &params->q_tails[curr_level],
                         obj);
    params->q_n_objs[curr_level] -= 1;
    params->q_n_bytes[curr_level] -= obj->obj_size;

    prepend_obj_to_head(&params->q_heads[next_level], &params->q_tails[next_level],
                        obj);
    params->q_n_objs[next_level] += 1;
    params->q_n_bytes[next_level] += obj->obj_size;
    obj->SLRU.lru_id = next_level;
  } else {
    move_obj_to_head(&params->q_heads[curr_level], &params->q_tails[curr_level],
                     obj);
  }

  return obj;
}

static cache_obj_t *MQ_insert(cache_t *cache, const request_t *req) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  cache_obj_t *obj = cache_insert_base(cache, req);

  obj->misc.freq = 1;
  obj->SLRU.lru_id = 0;
  prepend_obj_to_head(&params->q_heads[0], &params->q_tails[0], obj);
  params->q_n_objs[0] += 1;
  params->q_n_bytes[0] += obj->obj_size;

  return obj;
}

static cache_obj_t *MQ_to_evict(cache_t *cache, const request_t *req) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  (void)req;

  for (int i = 0; i < params->n_queue; i++) {
    if (params->q_tails[i] != NULL) {
      cache->to_evict_candidate_gen_vtime = cache->n_req;
      return params->q_tails[i];
    }
  }

  DEBUG_ASSERT(cache->occupied_byte == 0);
  return NULL;
}

static void MQ_evict(cache_t *cache, const request_t *req) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  cache_obj_t *obj_to_evict = MQ_to_evict(cache, req);
  DEBUG_ASSERT(obj_to_evict != NULL);

  int level = obj_to_evict->SLRU.lru_id;
  remove_obj_from_list(&params->q_heads[level], &params->q_tails[level],
                       obj_to_evict);
  params->q_n_objs[level] -= 1;
  params->q_n_bytes[level] -= obj_to_evict->obj_size;

  cache_evict_base(cache, obj_to_evict, true);
}

static void MQ_remove_obj(cache_t *cache, cache_obj_t *obj) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;
  int level = obj->SLRU.lru_id;

  remove_obj_from_list(&params->q_heads[level], &params->q_tails[level], obj);
  params->q_n_objs[level] -= 1;
  params->q_n_bytes[level] -= obj->obj_size;

  cache_remove_obj_base(cache, obj, true);
}

static bool MQ_remove(cache_t *cache, obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  MQ_remove_obj(cache, obj);
  return true;
}

static void MQ_parse_params(cache_t *cache, const char *cache_specific_params) {
  MQ_params_t *params = (MQ_params_t *)cache->eviction_params;

  char *params_str = strdup(cache_specific_params);
  char *params_str_to_free = params_str;

  while (params_str != NULL && params_str[0] != '\0') {
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    if (strcasecmp(key, "n-queue") == 0 || strcasecmp(key, "n-queues") == 0 ||
        strcasecmp(key, "nq") == 0) {
      params->n_queue = (int)strtol(value, NULL, 0);
      if (params->n_queue <= 0 || params->n_queue > MQ_MAX_N_QUEUE) {
        ERROR("MultiQueue n-queue should be in [1, %d], given %d\n",
              MQ_MAX_N_QUEUE, params->n_queue);
        abort();
      }
    } else {
      WARN("MQ does not support parameter %s\n", key);
    }
  }

  free(params_str_to_free);
}

#ifdef __cplusplus
}
#endif
