//
//  segmented fifo implemented using multiple lists instead of multiple fifos
//  this has a better performance than LP_SFIFOv0, but it is very hard to
//  implement
//
//  LP_SFIFO.c
//  libCacheSim
//
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LP_SFIFO_MAX_N_SEG 16

typedef struct LP_SFIFO_params {
  cache_t **fifos;
  int64_t *per_seg_max_size;
  int n_seg;
  request_t *req_local;
} LP_SFIFO_params_t;

static const char *DEFAULT_PARAMS = "n-seg=4,seg-size=1:1:1:1";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
static void LP_SFIFO_parse_params(cache_t *cache,
                                 const char *cache_specific_params);
static void LP_SFIFO_parse_params(cache_t *cache,
                                 const char *cache_specific_params);
static void LP_SFIFO_free(cache_t *cache);
static bool LP_SFIFO_get(cache_t *cache, const request_t *req);
static cache_obj_t *LP_SFIFO_find(cache_t *cache, const request_t *req,
                                 const bool update_cache);
static cache_obj_t *LP_SFIFO_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LP_SFIFO_to_evict(cache_t *cache, const request_t *req);
static void LP_SFIFO_evict(cache_t *cache, const request_t *req);
static bool LP_SFIFO_remove(cache_t *cache, const obj_id_t obj_id);

static inline bool LP_SFIFO_can_insert(cache_t *cache, const request_t *req);
static inline int64_t LP_SFIFO_get_occupied_byte(const cache_t *cache);
static inline int64_t LP_SFIFO_get_n_obj(const cache_t *cache);
static void LP_SFIFO_demote(cache_t *cache, const request_t *req, int seg_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************
/**
 * @brief initialize a LP_SFIFO cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params LP_SFIFO specific parameters, e.g., "n-seg=4"
 */
cache_t *LP_SFIFO_init(const common_cache_params_t ccache_params,
                      const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("LP-SFIFO", ccache_params, cache_specific_params);
  cache->cache_init = LP_SFIFO_init;
  cache->cache_free = LP_SFIFO_free;
  cache->get = LP_SFIFO_get;
  cache->find = LP_SFIFO_find;
  cache->insert = LP_SFIFO_insert;
  cache->evict = LP_SFIFO_evict;
  cache->remove = LP_SFIFO_remove;
  cache->to_evict = LP_SFIFO_to_evict;
  cache->get_occupied_byte = LP_SFIFO_get_occupied_byte;
  cache->get_n_obj = LP_SFIFO_get_n_obj;
  cache->can_insert = LP_SFIFO_can_insert;

  cache->obj_md_size = 0;

  cache->eviction_params = (LP_SFIFO_params_t *)malloc(sizeof(LP_SFIFO_params_t));
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)(cache->eviction_params);
  memset(params, 0, sizeof(LP_SFIFO_params_t));
  params->req_local = new_request();

  // prepare the default parameters
  LP_SFIFO_parse_params(cache, DEFAULT_PARAMS);
  if (cache_specific_params != NULL) {
    LP_SFIFO_parse_params(cache, cache_specific_params);
  }

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.hashpower -= 2;
  params->fifos = malloc(sizeof(cache_t *) * params->n_seg);
  
  for (int i = 0; i < params->n_seg; i++) {
    ccache_params_local.cache_size = params->per_seg_max_size[i];
    params->fifos[i] = FIFO_init(ccache_params_local, NULL);
  }

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void LP_SFIFO_free(cache_t *cache) {
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)(cache->eviction_params);
  free_request(params->req_local);
  for (int i = 0; i < params->n_seg; i++) {
    params->fifos[i]->cache_free(params->fifos[i]);
  }
  free(params->fifos);
  free(params->per_seg_max_size);
  free(params);
  cache_struct_free(cache);
}

/**
 * @brief this function is the user facing API
 * it performs the following logic
 *
 * ```
 * if obj in cache:
 *    update_metadata
 *    return true
 * else:
 *    if cache does not have enough space:
 *        evict until it has space to insert
 *    insert the object
 *    return false
 * ```
 *
 * @param cache
 * @param req
 * @return true if cache hit, false if cache miss
 */
static bool LP_SFIFO_get(cache_t *cache, const request_t *req) {
  // LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)(cache->eviction_params);
  // printf("%ld %ld %ld %ld %ld\n", cache->n_req,
  //        params->fifos[0]->get_n_obj(params->fifos[0]),
  //        params->fifos[1]->get_n_obj(params->fifos[1]),
  //        params->fifos[2]->get_n_obj(params->fifos[2]),
  //        params->fifos[3]->get_n_obj(params->fifos[3]));
  return cache_get_base(cache, req);
}

// ***********************************************************************
// ****                                                               ****
// ****       developer facing APIs (used by cache developer)         ****
// ****                                                               ****
// ***********************************************************************
/**
 * @brief find an object in the cache
 *
 * @param cache
 * @param req
 * @param update_cache whether to update the cache,
 *  if true, the object is promoted
 *  and if the object is expired, it is removed from the cache
 * @return the object or NULL if not found
 */
static cache_obj_t *LP_SFIFO_find(cache_t *cache, const request_t *req,
                                 const bool update_cache) {
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)(cache->eviction_params);
  cache_obj_t *obj = NULL;
  for (int i = 0; i < params->n_seg; i++) {
    obj = params->fifos[i]->find(params->fifos[i], req, false);
    if (obj != NULL) {
      // TODO: can distinguish hits on different segments
      if (i == 0) {
        if (obj->SFIFO.freq == 0) {
          obj->SFIFO.freq = 1;
        }
      } else {
        obj->SFIFO.freq += 1;
      }

      return obj;
    }
  }

  return NULL;
}

/**
 * @brief insert an object into the cache,
 * update the hash table and cache metadata
 * this function assumes the cache has enough space
 * eviction should be
 * performed before calling this function
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
cache_obj_t *LP_SFIFO_insert(cache_t *cache, const request_t *req) {
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)(cache->eviction_params);

  // Find the lowest fifo with space for insertion
  int nth_seg = -1;
  for (int i = 0; i < params->n_seg; i++) {
    if (params->fifos[i]->get_occupied_byte(params->fifos[i]) + req->obj_size +
            cache->obj_md_size <=
        params->per_seg_max_size[i]) {
      nth_seg = i;
      break;
    }
  }

  if (nth_seg == -1) {
    // No space for insertion
    while (cache->occupied_byte + req->obj_size + cache->obj_md_size >
           cache->cache_size) {
      cache->evict(cache, req);
    }
    nth_seg = 0;
  }

  // cache_obj_t *obj = hashtable_insert(cache->hashtable, req);
  cache_obj_t *obj =
      params->fifos[nth_seg]->insert(params->fifos[nth_seg], req);
  obj->SFIFO.freq = 0;

  return obj;
}

/**
 * @brief find the object to be evicted
 * this function does not actually evict the object or update metadata
 * not all eviction algorithms support this function
 * because the eviction logic cannot be decoupled from finding eviction
 * candidate, so use assert(false) if you cannot support this function
 *
 * @param cache the cache
 * @return the object to be evicted
 */
static cache_obj_t *LP_SFIFO_to_evict(cache_t *cache, const request_t *req) {
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)(cache->eviction_params);
  for (int i = 0; i < params->n_seg; i++) {
    if (params->fifos[i]->get_occupied_byte(params->fifos[i]) > 0) {
      return params->fifos[i]->to_evict(params->fifos[i], req);
    }
  }
  return NULL;
}

/**
 * @brief evict an object from the cache
 * it needs to call cache_evict_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param req not used
 * @param evicted_obj if not NULL, return the evicted object to caller
 */
static void LP_SFIFO_evict(cache_t *cache, const request_t *req) {
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)(cache->eviction_params);

  cache_obj_t *obj = NULL;
  assert(params->fifos[0]->get_occupied_byte(params->fifos[0]) > 0);

  obj = params->fifos[0]->to_evict(params->fifos[0], req);
  if (obj->SFIFO.freq > 0) {
    // insert to the next segment
    int seg_id_to_upsert = obj->SFIFO.freq;
    if (seg_id_to_upsert >= params->n_seg) {
      seg_id_to_upsert = params->n_seg - 1;
    }
    copy_cache_obj_to_request(params->req_local, obj);
    params->fifos[0]->evict(params->fifos[0], req);
    // if (seg_id_to_upsert != 1)
    //   printf("insert to %d\n", seg_id_to_upsert);

    cache_t *fifo_insert = params->fifos[seg_id_to_upsert];
    obj = fifo_insert->insert(fifo_insert, params->req_local);
    obj->SFIFO.freq = 0;

    if (fifo_insert->get_occupied_byte(fifo_insert) >
        params->per_seg_max_size[seg_id_to_upsert])
      LP_SFIFO_demote(cache, req, seg_id_to_upsert);
  } else {
    params->fifos[0]->evict(params->fifos[0], req);
  }
}

static void LP_SFIFO_demote(cache_t *cache, const request_t *req, int seg_id) {
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)(cache->eviction_params);
  cache_obj_t *obj = NULL;

  if (seg_id == 0) {
    assert(cache->get_occupied_byte(cache) <= cache->cache_size);
    return;
  }

  cache_t *curr_fifo = params->fifos[seg_id];
  cache_t *next_fifo = params->fifos[seg_id - 1];

  while (curr_fifo->get_occupied_byte(curr_fifo) >
         params->per_seg_max_size[seg_id]) {
    obj = curr_fifo->to_evict(curr_fifo, req);
    int freq = obj->SFIFO.freq;
    copy_cache_obj_to_request(params->req_local, obj);
    curr_fifo->evict(curr_fifo, req);

    obj = next_fifo->insert(next_fifo, params->req_local);
    obj->SFIFO.freq = freq;
  }
  if (next_fifo->get_occupied_byte(next_fifo) >
      params->per_seg_max_size[seg_id - 1]) {
    return LP_SFIFO_demote(cache, req, seg_id - 1);
  }
}

/**
 * @brief remove an object from the cache
 * this is different from cache_evict because it is used to for user trigger
 * remove, and eviction is used by the cache to make space for new objects
 *
 * it needs to call cache_remove_obj_base before returning
 * which updates some metadata such as n_obj, occupied size, and hash table
 *
 * @param cache
 * @param obj_id
 * @return true if the object is removed, false if the object is not in the
 * cache
 */
static bool LP_SFIFO_remove(cache_t *cache, const obj_id_t obj_id) {
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)(cache->eviction_params);

  for (int i = 0; i < params->n_seg; i++) {
    if (params->fifos[i]->remove(params->fifos[i], obj_id)) {
      return true;
    }
  }

  return false;
}

// ***********************************************************************
// ****                                                               ****
// ****                     setup functions                           ****
// ****                                                               ****
// ***********************************************************************
static const char *LP_SFIFO_current_params(cache_t *cache,
                                          LP_SFIFO_params_t *params) {
  static __thread char params_str[128];
  int n =
      snprintf(params_str, 128, "n-seg=%d;seg-size=%d\n", params->n_seg,
               (int)(params->per_seg_max_size[0] * 100 / cache->cache_size));

  for (int i = 1; i < params->n_seg; i++) {
    n += snprintf(params_str + n, 128 - n, ":%d",
                  (int)(params->per_seg_max_size[i] * 100 / cache->cache_size));
  }
  snprintf(cache->cache_name + n, 128 - n, ")");

  return params_str;
}

static void LP_SFIFO_parse_params(cache_t *cache,
                                 const char *cache_specific_params) {
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)cache->eviction_params;
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
    } else if (strcasecmp(key, "seg-size") == 0) {
      int n_seg = 0;
      int64_t seg_size_sum = 0;
      int64_t seg_size_array[LP_SFIFO_MAX_N_SEG];
      char *v = strsep((char **)&value, ":");
      while (v != NULL) {
        seg_size_array[n_seg++] = (int64_t)strtol(v, &end, 0);
        seg_size_sum += seg_size_array[n_seg - 1];
        v = strsep((char **)&value, ":");
      }
      params->n_seg = n_seg;
      if (params->per_seg_max_size != NULL) {
        free(params->per_seg_max_size);
      }
      params->per_seg_max_size = calloc(params->n_seg, sizeof(int64_t));
      for (int i = 0; i < n_seg; i++) {
        params->per_seg_max_size[i] = (int64_t)(
            (double)seg_size_array[i] / seg_size_sum * cache->cache_size);
      }
    } else if (strcasecmp(key, "print") == 0) {
      printf("current parameters: %s\n", LP_SFIFO_current_params(cache, params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }
  free(old_params_str);
}

// ***********************************************************************
// ****                                                               ****
// ****                   internal functions                          ****
// ****                                                               ****
// ***********************************************************************

/* LP_SFIFO cannot an object larger than segment size */
static inline bool LP_SFIFO_can_insert(cache_t *cache, const request_t *req) {
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)cache->eviction_params;
  bool can_insert = cache_can_insert_default(cache, req);
  return can_insert &&
         (req->obj_size + cache->obj_md_size <= params->fifos[0]->cache_size);
}

static inline int64_t LP_SFIFO_get_occupied_byte(const cache_t *cache) {
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)cache->eviction_params;
  int64_t occupied_byte = 0;
  for (int i = 0; i < params->n_seg; i++) {
    occupied_byte += params->fifos[i]->get_occupied_byte(params->fifos[i]);
  }
  return occupied_byte;
}

static inline int64_t LP_SFIFO_get_n_obj(const cache_t *cache) {
  LP_SFIFO_params_t *params = (LP_SFIFO_params_t *)cache->eviction_params;
  int64_t n_obj = 0;
  for (int i = 0; i < params->n_seg; i++) {
    n_obj += params->fifos[i]->get_occupied_byte(params->fifos[i]);
  }
  return n_obj;
}


#ifdef __cplusplus
extern "C"
}
#endif