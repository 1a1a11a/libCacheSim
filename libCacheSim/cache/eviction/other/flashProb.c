//
//  LRU cache in DRAM and use probability to admit objects to flash
//  this is used to calculate the write amplification 
//
//
//  flashProb.c
//  libCacheSim
//
//  Created by Juncheng on 2/24/23.
//  Copyright © 2018 Juncheng. All rights reserved.
//

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/evictionAlgo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  cache_t *ram;
  cache_t *disk;

  int64_t n_obj_admit_to_ram;
  int64_t n_obj_admit_to_disk;
  int64_t n_byte_admit_to_ram;
  int64_t n_byte_admit_to_disk;

  double ram_size_ratio;
  double disk_admit_prob;
  int inv_prob;

  char ram_cache_type[16];
  char disk_cache_type[16];
  request_t *req_local;
} flashProb_params_t;

static const char *DEFAULT_CACHE_PARAMS =
    "ram-size-ratio=0.05,disk-admit-prob=0.2,ram-cache=LRU,disk-cache=FIFO";

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************
cache_t *flashProb_init(const common_cache_params_t ccache_params,
                        const char *cache_specific_params);
static void flashProb_free(cache_t *cache);
static bool flashProb_get(cache_t *cache, const request_t *req);

static cache_obj_t *flashProb_find(cache_t *cache, const request_t *req,
                                   const bool update_cache);
static cache_obj_t *flashProb_insert(cache_t *cache, const request_t *req);
static cache_obj_t *flashProb_to_evict(cache_t *cache, const request_t *req);
static void flashProb_evict(cache_t *cache, const request_t *req);
static bool flashProb_remove(cache_t *cache, const obj_id_t obj_id);
static inline int64_t flashProb_get_occupied_byte(const cache_t *cache);
static inline int64_t flashProb_get_n_obj(const cache_t *cache);
static void flashProb_parse_params(cache_t *cache,
                                   const char *cache_specific_params);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ***********************************************************************

cache_t *flashProb_init(const common_cache_params_t ccache_params,
                        const char *cache_specific_params) {
  cache_t *cache = cache_struct_init("flashProb", ccache_params, cache_specific_params);
  cache->cache_init = flashProb_init;
  cache->cache_free = flashProb_free;
  cache->get = flashProb_get;
  cache->find = flashProb_find;
  cache->insert = flashProb_insert;
  cache->evict = flashProb_evict;
  cache->remove = flashProb_remove;
  cache->to_evict = flashProb_to_evict;
  cache->get_n_obj = flashProb_get_n_obj;
  cache->get_occupied_byte = flashProb_get_occupied_byte;

  cache->obj_md_size = 0;

  cache->eviction_params = malloc(sizeof(flashProb_params_t));
  memset(cache->eviction_params, 0, sizeof(flashProb_params_t));
  flashProb_params_t *params = (flashProb_params_t *)cache->eviction_params;
  params->req_local = new_request();

  flashProb_parse_params(cache, DEFAULT_CACHE_PARAMS);
  if (cache_specific_params != NULL) {
    flashProb_parse_params(cache, cache_specific_params);
  }

  int64_t ram_cache_size =
      (int64_t)ccache_params.cache_size * params->ram_size_ratio;
  int64_t disk_cache_size = ccache_params.cache_size - ram_cache_size;

  common_cache_params_t ccache_params_local = ccache_params;
  ccache_params_local.cache_size = ram_cache_size;
  if (strcasecmp(params->ram_cache_type, "ARC") == 0) {
    params->ram = ARC_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->ram_cache_type, "LHD") == 0) {
    params->ram = LHD_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->ram_cache_type, "clock") == 0) {
    params->ram = Clock_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->ram_cache_type, "fifo") == 0) {
    params->ram = FIFO_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->ram_cache_type, "clock-2") == 0) {
    params->ram = Clock_init(ccache_params_local, "n-bit-counter=2");
  } else if (strcasecmp(params->ram_cache_type, "clock-3") == 0) {
    params->ram = Clock_init(ccache_params_local, "n-bit-counter=3");
  } else if (strcasecmp(params->ram_cache_type, "LRU") == 0) {
    params->ram = LRU_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->ram_cache_type, "LeCaR") == 0) {
    params->ram = LeCaR_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->ram_cache_type, "Cacheus") == 0) {
    params->ram = Cacheus_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->ram_cache_type, "twoQ") == 0) {
    params->ram = TwoQ_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->ram_cache_type, "FIFO") == 0) {
    params->ram = FIFO_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->ram_cache_type, "LIRS") == 0) {
    params->ram = LIRS_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->ram_cache_type, "Hyperbolic") == 0) {
    params->ram = Hyperbolic_init(ccache_params_local, NULL);
  } else {
    ERROR("flashProb does not support %s\n", params->ram_cache_type);
  }

  ccache_params_local.cache_size = disk_cache_size;
  if (strcasecmp(params->disk_cache_type, "fifo") == 0) {
    params->disk = FIFO_init(ccache_params_local, NULL);
  } else if (strcasecmp(params->disk_cache_type, "clock") == 0) {
    params->disk = Clock_init(ccache_params_local, NULL);
  } else {
    ERROR("flashProb does not support %s\n", params->disk_cache_type);
  }

  snprintf(cache->cache_name, CACHE_NAME_ARRAY_LEN, "flashProb-%.4lf-%s-%.4lf-%s",
           params->ram_size_ratio, 
           params->ram_cache_type, 
           params->disk_admit_prob,
           params->disk_cache_type);

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void flashProb_free(cache_t *cache) {
  flashProb_params_t *params = (flashProb_params_t *)cache->eviction_params;
  free_request(params->req_local);
  params->ram->cache_free(params->ram);
  params->disk->cache_free(params->disk);
  free(cache->eviction_params);
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
static bool flashProb_get(cache_t *cache, const request_t *req) {

  bool cache_hit = cache_get_base(cache, req);

  return cache_hit;
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
static cache_obj_t *flashProb_find(cache_t *cache, const request_t *req,
                                   const bool update_cache) {
  flashProb_params_t *params = (flashProb_params_t *)cache->eviction_params;

  // if update cache is false, we only check the ram and disk caches
  if (!update_cache) {
    cache_obj_t *obj = params->ram->find(params->ram, req, false);
    if (obj != NULL) {
      return obj;
    }
    obj = params->disk->find(params->disk, req, false);
    if (obj != NULL) {
      return obj;
    }
    return NULL;
  }

  cache_obj_t *obj = params->ram->find(params->ram, req, true);
  if (obj != NULL) {
    return obj;
  }

  obj = params->disk->find(params->disk, req, true);

  return obj;
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
static cache_obj_t *flashProb_insert(cache_t *cache, const request_t *req) {
  flashProb_params_t *params = (flashProb_params_t *)cache->eviction_params;
  cache_obj_t *obj = NULL;

  /* insert into the ram */
  // if (req->obj_size >= params->ram->cache_size) {
  //   return NULL;
  // }
  params->n_obj_admit_to_ram += 1;
  params->n_byte_admit_to_ram += req->obj_size;
  obj = params->ram->insert(params->ram, req);

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
static cache_obj_t *flashProb_to_evict(cache_t *cache, const request_t *req) {
  assert(false);
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
static void flashProb_evict(cache_t *cache, const request_t *req) {
  flashProb_params_t *params = (flashProb_params_t *)cache->eviction_params;

  cache_t *ram = params->ram;
  cache_t *disk = params->disk;

  if (ram->get_occupied_byte(ram) == 0) {
    assert(disk->get_occupied_byte(disk) <= cache->cache_size);
    // evict from disk cache
    disk->evict(disk, req);

    return;
  }

  // evict from RAM
  cache_obj_t *obj = ram->to_evict(ram, req);
  assert(obj != NULL);
  // need to copy the object before it is evicted
  copy_cache_obj_to_request(params->req_local, obj);

  // remove from RAM, but do not update stat
  ram->remove(ram, params->req_local->obj_id);

  // freq is updated in cache_find_base
  if (next_rand() % params->inv_prob == 0) {
    params->n_obj_admit_to_disk += 1;
    params->n_byte_admit_to_disk += obj->obj_size;
    // get will insert to and evict from disk cache
    params->disk->get(params->disk, params->req_local);
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
static bool flashProb_remove(cache_t *cache, const obj_id_t obj_id) {
  flashProb_params_t *params = (flashProb_params_t *)cache->eviction_params;
  bool removed = false;
  removed = removed || params->ram->remove(params->ram, obj_id);
  removed = removed || params->disk->remove(params->disk, obj_id);

  return removed;
}

static inline int64_t flashProb_get_occupied_byte(const cache_t *cache) {
  flashProb_params_t *params = (flashProb_params_t *)cache->eviction_params;
  return params->ram->get_occupied_byte(params->ram) +
         params->disk->get_occupied_byte(params->disk);
}

static inline int64_t flashProb_get_n_obj(const cache_t *cache) {
  flashProb_params_t *params = (flashProb_params_t *)cache->eviction_params;
  return params->ram->get_n_obj(params->ram) +
         params->disk->get_n_obj(params->disk);
}

// static inline bool flashProb_can_insert(cache_t *cache, const request_t *req) {
//   flashProb_params_t *params = (flashProb_params_t *)cache->eviction_params;

//   return req->obj_size <= params->fifo->cache_size;
// }

// ***********************************************************************
// ****                                                               ****
// ****                parameter set up functions                     ****
// ****                                                               ****
// ***********************************************************************
static const char *flashProb_current_params(flashProb_params_t *params) {
  static __thread char params_str[128];
  snprintf(params_str, 128, "ram-size-ratio=%.4lf,disk-admit-prob=%.4lf,ram-cache=%s\n",
           params->ram_size_ratio, params->disk_admit_prob, params->ram->cache_name);
  return params_str;
}

static void flashProb_parse_params(cache_t *cache,
                                   const char *cache_specific_params) {
  flashProb_params_t *params = (flashProb_params_t *)(cache->eviction_params);

  char *params_str = strdup(cache_specific_params);
  char *old_params_str = params_str;

  while (params_str != NULL && params_str[0] != '\0') {
    /* different parameters are separated by comma,
     * key and value are separated by = */
    char *key = strsep((char **)&params_str, "=");
    char *value = strsep((char **)&params_str, ",");

    // skip the white space
    while (params_str != NULL && *params_str == ' ') {
      params_str++;
    }

    if (strcasecmp(key, "ram-size-ratio") == 0) {
      params->ram_size_ratio = strtod(value, NULL);
    } else if (strcasecmp(key, "disk-admit-prob") == 0) {
      params->disk_admit_prob = strtod(value, NULL);
      params->inv_prob = (int) (1.0 / params->disk_admit_prob);
    } else if (strcasecmp(key, "ram-cache") == 0) {
      strncpy(params->ram_cache_type, value, 15);
    } else if (strcasecmp(key, "disk-cache") == 0) {
      strncpy(params->disk_cache_type, value, 15);
    } else if (strcasecmp(key, "print") == 0) {
      printf("parameters: %s\n", flashProb_current_params(params));
      exit(0);
    } else {
      ERROR("%s does not have parameter %s\n", cache->cache_name, key);
      exit(1);
    }
  }

  free(old_params_str);
}

#ifdef __cplusplus
}
#endif
