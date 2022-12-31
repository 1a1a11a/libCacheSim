/**
 * @file LPQD.c
 * @brief Implementation of LPQD eviction algorithm
 * LPQD: lazy promotion and quick demotion
 * it uses a probationary FIFO queue with a clock cache
 * objects are first inserted into the FIFO queue, and moved to the clock cache
 * upon the second request
 * if the cache is full, evict from the FIFO cache
 * if the clock cache is full, then promoting to clock will trigger an eviction
 * from the clock cache (not insert into the FIFO queue)
 */

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/evictionAlgo/priv/LPQD.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_obj_t *fifo_head;
  cache_obj_t *fifo_tail;

  cache_obj_t *fifo_ghost_head;
  cache_obj_t *fifo_ghost_tail;

  cache_obj_t *clock_head;
  cache_obj_t *clock_tail;
  int64_t n_fifo_obj;
  int64_t n_fifo_byte;
  int64_t n_clock_obj;
  int64_t n_clock_byte;
  // the user specified size
  double fifo_ratio;
  int64_t fifo_size;
  int64_t clock_size;
  cache_obj_t *clock_pointer;
} LPQD_params_t;

void LPQD_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove);

void LPQD_clock_evict(cache_t *cache, const request_t *req);

bool LPQD_check(cache_t *cache, const request_t *req, const bool update_cache) {
  cache_obj_t *cache_obj;
  bool cache_hit = cache_check_base(cache, req, update_cache, &cache_obj);
  if (cache_obj != NULL && update_cache) {
    if (cache_obj->LPQD.cache_id == 1) {
      // FIFO cache, promote to clock cache
      LPQD_params_t *params = cache->eviction_params;
      remove_obj_from_list(&params->fifo_head, &params->fifo_tail, cache_obj);
      params->n_fifo_obj--;
      params->n_fifo_byte -= cache_obj->obj_size + cache->obj_md_size;
      prepend_obj_to_head(&params->clock_head, &params->clock_tail, cache_obj);
      params->n_clock_obj++;
      params->n_clock_byte += cache_obj->obj_size + cache->obj_md_size;
      cache_obj->LPQD.cache_id = 2;
      if (params->n_clock_byte > params->clock_size) {
        // clock cache is full, evict from clock cache
        LPQD_clock_evict(cache, req);
      }

    } else if (cache_obj->LPQD.cache_id == 2) {
      // clock cache
      cache_obj->LPQD.visited = true;
    } else {
      ERROR("cache_obj->LPQD.cache_id = %d", cache_obj->LPQD.cache_id);
    }
  }

  return cache_hit;
}

bool LPQD_get(cache_t *cache, const request_t *req) {
  return cache_get_base(cache, req);
}

cache_obj_t *LPQD_insert(cache_t *cache, const request_t *req) {
  LPQD_params_t *params = cache->eviction_params;

  cache_obj_t *obj = cache_insert_base(cache, req);
  if (params->fifo_head == NULL) {
    DEBUG_ASSERT(params->fifo_tail == NULL);
    params->fifo_tail = obj;
  } else {
    params->fifo_head->queue.prev = obj;
    obj->queue.next = params->fifo_head;
  }

  params->fifo_head = obj;
  params->n_fifo_obj++;
  params->n_fifo_byte += obj->obj_size + cache->obj_md_size;
  obj->LPQD.cache_id = 1;
  obj->LPQD.visited = false;

  return obj;
}

cache_obj_t *LPQD_to_evict(cache_t *cache) {
  LPQD_params_t *params = cache->eviction_params;
  if (params->fifo_tail != NULL) {
    return params->fifo_tail;
  } else {
    // not implemented, we need to evict from the clock cache
    assert(0);
  }
}

void LPQD_clock_evict(cache_t *cache, const request_t *req) {
  LPQD_params_t *params = cache->eviction_params;
  cache_obj_t *moving_clock_pointer = params->clock_pointer;

  /* if we have run one full around or first eviction */
  if (moving_clock_pointer == NULL) moving_clock_pointer = params->clock_tail;

  /* find the first untouched */
  while (moving_clock_pointer != NULL && moving_clock_pointer->LPQD.visited) {
    moving_clock_pointer->LPQD.visited = false;
    moving_clock_pointer = moving_clock_pointer->queue.prev;
  }

  /* if we have finished one around, start from the tail */
  if (moving_clock_pointer == NULL) {
    moving_clock_pointer = params->clock_tail;
    while (moving_clock_pointer != NULL && moving_clock_pointer->LPQD.visited) {
      moving_clock_pointer->LPQD.visited = false;
      moving_clock_pointer = moving_clock_pointer->queue.prev;
    }
  }

  params->clock_pointer = moving_clock_pointer->queue.prev;
  LPQD_remove_obj(cache, moving_clock_pointer);
}

void LPQD_evict(cache_t *cache, const request_t *req,
                cache_obj_t *evicted_obj) {
  LPQD_params_t *params = cache->eviction_params;
  cache_obj_t *obj_to_evict = params->fifo_tail;

  if (evicted_obj != NULL) {
    // return evicted object to caller
    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
  }

  LPQD_remove_obj(cache, obj_to_evict);
}

void LPQD_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  DEBUG_ASSERT(obj_to_remove != NULL);
  LPQD_params_t *params = cache->eviction_params;

  if (obj_to_remove->LPQD.cache_id == 1) {
    // fifo cache
    remove_obj_from_list(&params->fifo_head, &params->fifo_tail, obj_to_remove);
    params->n_fifo_obj--;
    params->n_fifo_byte -= obj_to_remove->obj_size + cache->obj_md_size;
  } else if (obj_to_remove->LPQD.cache_id == 2) {
    // clock cache
    remove_obj_from_list(&params->clock_head, &params->clock_tail, obj_to_remove);
    params->n_clock_obj--;
    params->n_clock_byte -= obj_to_remove->obj_size + cache->obj_md_size;
  }

  cache_remove_obj_base(cache, obj_to_remove);
}

bool LPQD_remove(cache_t *cache, const obj_id_t obj_id) {
  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }
  LPQD_remove_obj(cache, obj);

  return true;
}

// ######################## setup function ###########################
static const char *LPQD_current_params(LPQD_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "fifo-ratio=%.4lf\n", params->fifo_ratio);
  return params_str;
}

static void LPQD_parse_params(cache_t *cache,
                              const char *cache_specific_params) {
  LPQD_params_t *params = (LPQD_params_t *)cache->eviction_params;
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

    if (strcasecmp(key, "fifo-ratio") == 0) {
      params->fifo_ratio = (double)strtof(value, &end);
      if (strlen(end) > 2) {
        ERROR("param parsing error, find string \"%s\" after number\n", end);
      }

    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", LPQD_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }
  free(old_params_str);
}

cache_t *LPQD_init(const common_cache_params_t ccache_params,
                   const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LPQD", ccache_params);
  cache->cache_init = LPQD_init;
  cache->cache_free = LPQD_free;
  cache->get = LPQD_get;
  cache->check = LPQD_check;
  cache->insert = LPQD_insert;
  cache->evict = LPQD_evict;
  cache->remove = LPQD_remove;
  cache->to_evict = LPQD_to_evict;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->obj_md_size = 1;
  } else {
    cache->obj_md_size = 0;
  }

  cache->eviction_params = my_malloc(LPQD_params_t);
  LPQD_params_t *params = (LPQD_params_t *)cache->eviction_params;
  memset(cache->eviction_params, 0, sizeof(LPQD_params_t));
  params->clock_pointer = NULL;
  params->fifo_ratio = 0.2;
  params->n_fifo_obj = 0;
  params->n_fifo_byte = 0;
  params->n_clock_obj = 0;
  params->n_clock_byte = 0;
  params->fifo_head = NULL;
  params->fifo_tail = NULL;
  params->clock_head = NULL;
  params->clock_tail = NULL;
  params->fifo_ghost_head = NULL;
  params->fifo_ghost_tail = NULL;

  if (cache_specific_params != NULL) {
    LPQD_parse_params(cache, cache_specific_params);
  }

  params->fifo_size = cache->cache_size * params->fifo_ratio;
  params->clock_size = cache->cache_size - params->fifo_size;

  return cache;
}

void LPQD_free(cache_t *cache) { cache_struct_free(cache); }

#ifdef __cplusplus
}
#endif
