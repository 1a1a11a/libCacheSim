//
//  a FIFO module that supports different obj size
//
//  SFIFO_Belady.c
//  libCacheSim
//
//

#include "../../dataStructure/hashtable/hashtable.h"
#include "../../include/libCacheSim/dist.h"
#include "../../include/libCacheSim/evictionAlgo/SFIFO_Belady.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SFIFO_Belady_params {
  cache_t **FIFOs;
  int n_seg;
  request_t *temp_req;
} SFIFO_Belady_params_t;

static const char *SFIFO_Belady_current_params(SFIFO_Belady_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "n-seg=%d\n", params->n_seg);
  return params_str;
}

static void SFIFO_Belady_parse_params(cache_t *cache,
                                      const char *cache_specific_params) {
  SFIFO_Belady_params_t *params =
      (SFIFO_Belady_params_t *)cache->eviction_params;
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
      printf("current parameters: %s\n", SFIFO_Belady_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }
  free(old_params_str);
}

/**
 * @brief SLRU cannot insert an object larger than segment size
 *
 * @param cache
 * @param req
 * @return true
 * @return false
 */
bool SFIFO_Belady_can_insert(cache_t *cache, const request_t *req) {
  SFIFO_Belady_params_t *params =
      (SFIFO_Belady_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);
  return can_insert && (req->obj_size + cache->per_obj_metadata_size <=
                        params->FIFOs[0]->cache_size);
}

int64_t SFIFO_Belady_get_occupied_byte(const cache_t *cache) {
  SFIFO_Belady_params_t *params =
      (SFIFO_Belady_params_t *)cache->eviction_params;
  int64_t occupied_byte = 0;
  for (int i = 0; i < params->n_seg; i++) {
    occupied_byte += params->FIFOs[i]->occupied_size;
  }
  return occupied_byte;
}

int64_t SFIFO_Belady_get_n_obj(const cache_t *cache) {
  SFIFO_Belady_params_t *params =
      (SFIFO_Belady_params_t *)cache->eviction_params;
  int64_t n_obj = 0;
  for (int i = 0; i < params->n_seg; i++) {
    n_obj += params->FIFOs[i]->n_obj;
  }
  return n_obj;
}

cache_t *SFIFO_Belady_init(const common_cache_params_t ccache_params,
                           const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("SFIFO_Belady", ccache_params);
  cache->cache_init = SFIFO_Belady_init;
  cache->cache_free = SFIFO_Belady_free;
  cache->get = SFIFO_Belady_get;
  cache->check = SFIFO_Belady_check;
  cache->insert = SFIFO_Belady_insert;
  cache->evict = SFIFO_Belady_evict;
  cache->remove = SFIFO_Belady_remove;
  cache->to_evict = SFIFO_Belady_to_evict;
  cache->can_insert = SFIFO_Belady_can_insert;
  cache->get_occupied_byte = SFIFO_Belady_get_occupied_byte;
  cache->get_n_obj = SFIFO_Belady_get_n_obj;
  cache->init_params = cache_specific_params;

  if (ccache_params.consider_obj_metadata) {
    cache->per_obj_metadata_size = 8 * 2;
  } else {
    cache->per_obj_metadata_size = 0;
  }

  cache->eviction_params =
      (SFIFO_Belady_params_t *)malloc(sizeof(SFIFO_Belady_params_t));
  SFIFO_Belady_params_t *SFIFO_Belady_params =
      (SFIFO_Belady_params_t *)(cache->eviction_params);
  SFIFO_Belady_params->n_seg = 4;
  SFIFO_Belady_params->temp_req = new_request();

  if (cache_specific_params != NULL) {
    SFIFO_Belady_parse_params(cache, cache_specific_params);
  }

  SFIFO_Belady_params->FIFOs =
      (cache_t **)malloc(sizeof(cache_t *) * SFIFO_Belady_params->n_seg);

  common_cache_params_t ccache_params_local = ccache_params;

  ccache_params_local.cache_size /= SFIFO_Belady_params->n_seg;
  ccache_params_local.hashpower /= MIN(16, ccache_params_local.hashpower - 4);
  for (int i = 0; i < SFIFO_Belady_params->n_seg; i++) {
    SFIFO_Belady_params->FIFOs[i] = FIFO_init(ccache_params_local, NULL);
  }

  return cache;
}

void SFIFO_Belady_free(cache_t *cache) {
  SFIFO_Belady_params_t *SFIFO_Belady_params =
      (SFIFO_Belady_params_t *)(cache->eviction_params);
  free_request(SFIFO_Belady_params->temp_req);
  for (int i = 0; i < SFIFO_Belady_params->n_seg; i++) {
    FIFO_free(SFIFO_Belady_params->FIFOs[i]);
  }
  free(SFIFO_Belady_params->FIFOs);
  cache_struct_free(cache);
}

/**
 * @brief move an object from ith FIFO into (i-1)th FIFO, cool
 * (i-1)th FIFO if it is full
 * the index of the FIFO is ordered from the least recent being 0
 * to the most recent being n_seg-1
 *
 * @param cache
 * @param i
 */
void SFIFO_Belady_cool(cache_t *cache, int i) {
  cache_obj_t evicted_obj;
  SFIFO_Belady_params_t *SFIFO_Belady_params =
      (SFIFO_Belady_params_t *)(cache->eviction_params);
  FIFO_evict(SFIFO_Belady_params->FIFOs[i], NULL, &evicted_obj);

  if (i == 0) return;

  // If lower FIFOs are full
  while (SFIFO_Belady_params->FIFOs[i - 1]->occupied_size +
             evicted_obj.obj_size + cache->per_obj_metadata_size >
         SFIFO_Belady_params->FIFOs[i - 1]->cache_size)
    SFIFO_Belady_cool(cache, i - 1);

  copy_cache_obj_to_request(SFIFO_Belady_params->temp_req, &evicted_obj);
  FIFO_insert(SFIFO_Belady_params->FIFOs[i - 1], SFIFO_Belady_params->temp_req);
}

cache_ck_res_e SFIFO_Belady_check(cache_t *cache, const request_t *req,
                                  const bool update_cache) {
  SFIFO_Belady_params_t *SFIFO_Belady_params =
      (SFIFO_Belady_params_t *)(cache->eviction_params);

  for (int i = 0; i < SFIFO_Belady_params->n_seg; i++) {
    cache_ck_res_e ret =
        FIFO_check(SFIFO_Belady_params->FIFOs[i], req, update_cache);

    if (ret == cache_ck_hit) {
      // bump object from lower segment to upper segment;
      if (i != SFIFO_Belady_params->n_seg - 1 &&
          cache->future_stack_dist[cache->n_req - 1] != -1 &&
          cache->future_stack_dist[cache->n_req - 1] < cache->cache_size) {
        FIFO_remove(SFIFO_Belady_params->FIFOs[i], req->obj_id);

        // If the upper FIFO is full;
        while (SFIFO_Belady_params->FIFOs[i + 1]->occupied_size +
                   req->obj_size + cache->per_obj_metadata_size >
               SFIFO_Belady_params->FIFOs[i + 1]->cache_size)
          SFIFO_Belady_cool(cache, i + 1);

        FIFO_insert(SFIFO_Belady_params->FIFOs[i + 1], req);
      }
      return cache_ck_hit;
    } else if (ret == cache_ck_expired)
      return cache_ck_expired;
  }
  return cache_ck_miss;
}

cache_ck_res_e SFIFO_Belady_get(cache_t *cache, const request_t *req) {
  /* because this field cannot be updated in time since segment LRUs are
   * updated, so we should not use this field */
  DEBUG_ASSERT(cache->occupied_size == 0);

  return cache_get_base(cache, req);
}

cache_obj_t *SFIFO_Belady_insert(cache_t *cache, const request_t *req) {
  SFIFO_Belady_params_t *SFIFO_Belady_params =
      (SFIFO_Belady_params_t *)(cache->eviction_params);

  int i;
  cache_obj_t *cache_obj = NULL;

  // this is used by eviction age tracking
  SFIFO_Belady_params->FIFOs[0]->n_req = cache->n_req;

  // Find the lowest FIFO with space for insertion
  for (i = 0; i < SFIFO_Belady_params->n_seg; i++) {
    if (SFIFO_Belady_params->FIFOs[i]->occupied_size + req->obj_size +
            cache->per_obj_metadata_size <=
        SFIFO_Belady_params->FIFOs[i]->cache_size) {
      cache_obj = FIFO_insert(SFIFO_Belady_params->FIFOs[i], req);
      break;
    }
  }

  // If all FIFOs are filled, evict an obj from the lowest FIFO.
  if (i == SFIFO_Belady_params->n_seg) {
    while (SFIFO_Belady_params->FIFOs[0]->occupied_size + req->obj_size +
               cache->per_obj_metadata_size >
           SFIFO_Belady_params->FIFOs[0]->cache_size) {
      SFIFO_Belady_evict(cache, req, NULL);
    }
    cache_obj = FIFO_insert(SFIFO_Belady_params->FIFOs[0], req);
  }

  return cache_obj;
}

cache_obj_t *SFIFO_Belady_to_evict(cache_t *cache) {
  SFIFO_Belady_params_t *SFIFO_Belady_params =
      (SFIFO_Belady_params_t *)(cache->eviction_params);
  for (int i = 0; i < SFIFO_Belady_params->n_seg; i++) {
    if (SFIFO_Belady_params->FIFOs[i]->occupied_size > 0) {
      return FIFO_to_evict(SFIFO_Belady_params->FIFOs[i]);
    }
  }
}

void SFIFO_Belady_evict(cache_t *cache, const request_t *req,
                        cache_obj_t *evicted_obj) {
  SFIFO_Belady_params_t *SFIFO_Belady_params =
      (SFIFO_Belady_params_t *)(cache->eviction_params);

  int nth_seg_to_evict = 0;
  for (int i = 0; i < SFIFO_Belady_params->n_seg; i++) {
    if (SFIFO_Belady_params->FIFOs[i]->occupied_size > 0) {
      nth_seg_to_evict = i;
      break;
    }
  }

#ifdef TRACK_EVICTION_R_AGE
  record_eviction_age(
      cache,
      req->real_time -
          SFIFO_Belady_params->FIFOs[nth_seg_to_evict]->q_tail->create_time);
#endif
#ifdef TRACK_EVICTION_V_AGE
  record_eviction_age(
      cache,
      cache->n_req -
          SFIFO_Belady_params->FIFOs[nth_seg_to_evict]->q_tail->create_time);
#endif

  cache_evict_LRU(SFIFO_Belady_params->FIFOs[nth_seg_to_evict], req,
                  evicted_obj);
}

void SFIFO_Belady_remove(cache_t *cache, const obj_id_t obj_id) {
  SFIFO_Belady_params_t *SFIFO_Belady_params =
      (SFIFO_Belady_params_t *)(cache->eviction_params);
  cache_obj_t *obj;
  for (int i = 0; i < SFIFO_Belady_params->n_seg; i++) {
    obj = cache_get_obj_by_id(SFIFO_Belady_params->FIFOs[i], obj_id);
    if (obj) {
      remove_obj_from_list(&(SFIFO_Belady_params->FIFOs[i])->q_head,
                           &(SFIFO_Belady_params->FIFOs[i])->q_tail, obj);
      cache_remove_obj_base(SFIFO_Belady_params->FIFOs[i], obj);
      return;
    }
  }
  if (obj == NULL) {
    WARN("obj (%" PRIu64 ") to remove is not in the cache\n", obj_id);
    return;
  }
}

#ifdef __cplusplus
extern "C"
}
#endif
