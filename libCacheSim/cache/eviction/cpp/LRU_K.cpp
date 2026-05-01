/* LRU-K: Evict the object with the largest backward K-distance.
 *
 * Objects that have been accessed fewer than K times have an infinite
 * backward K-distance and are evicted first in FIFO order.
 * Among objects with >= K accesses, the one with the oldest K-th most
 * recent access time is evicted first.
 *
 * Reference: O'Neil, O'Neil, Weikum. "The LRU-K Page Replacement Algorithm
 * for Database Disk Buffering." ACM SIGMOD 1993.
 *
 * Parameters:
 *   k: the number of recent accesses to track (default: 2)
 */

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <list>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include "abstractRank.hpp"

namespace eviction {

class LRU_K {
 public:
  int k;

  /* Objects with < K accesses are held in a FIFO queue.
   * We use a doubly-linked list + map for O(1) removal. */
  std::list<cache_obj_t *> fifo_queue;
  std::unordered_map<cache_obj_t *, std::list<cache_obj_t *>::iterator>
      fifo_map;

  /* Objects with >= K accesses are held in a priority queue sorted by
   * their K-th most recent access time (ascending = evict first). */
  using pq_entry_t = std::pair<int64_t, obj_id_t>;
  std::set<pq_entry_t> pq;
  std::unordered_map<cache_obj_t *, int64_t> pq_map;  // obj -> K-th vtime

  /* Per-object access history: the K most recent request vtimes.
   * front() = oldest (K-th most recent), back() = most recent. */
  std::unordered_map<cache_obj_t *, std::deque<int64_t>> history;

  explicit LRU_K(int k_param = 2) : k(k_param) {}
};

}  // namespace eviction

#ifdef __cplusplus
extern "C" {
#endif

// ***********************************************************************
// ****                                                               ****
// ****                   function declarations                       ****
// ****                                                               ****
// ***********************************************************************

cache_t *LRU_K_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params);
static void LRU_K_free(cache_t *cache);
static bool LRU_K_get(cache_t *cache, const request_t *req);

static cache_obj_t *LRU_K_find(cache_t *cache, const request_t *req,
                                bool update_cache);
static cache_obj_t *LRU_K_insert(cache_t *cache, const request_t *req);
static cache_obj_t *LRU_K_to_evict(cache_t *cache, const request_t *req);
static void LRU_K_evict(cache_t *cache, const request_t *req);
static bool LRU_K_remove(cache_t *cache, obj_id_t obj_id);

// ***********************************************************************
// ****                                                               ****
// ****                   end user facing functions                   ****
// ****                                                               ****
// ****                       init, free, get                         ****
// ***********************************************************************

/**
 * @brief parse algorithm-specific parameters
 *
 * Supported parameters (comma-separated key=value pairs):
 *   k=<int>   number of accesses to track (default: 2, must be >= 1)
 */
static void LRU_K_parse_params(cache_t *cache,
                               const char *cache_specific_params) {
  auto *lruk = reinterpret_cast<eviction::LRU_K *>(cache->eviction_params);
  if (cache_specific_params == nullptr || cache_specific_params[0] == '\0')
    return;

  char *params_str = strdup(cache_specific_params);
  char *to_free = params_str;
  char *end = nullptr;

  while (params_str != nullptr && params_str[0] != '\0') {
    char *key = strsep(&params_str, "=");
    char *value = strsep(&params_str, ",");

    while (params_str != nullptr && *params_str == ' ') params_str++;

    if (strcasecmp(key, "k") == 0) {
      if (value == nullptr || value[0] == '\0') {
        ERROR("LRU_K: missing value for k\n");
      }
      long k_val = strtol(value, &end, 0);
      if (end == value || (end != nullptr && *end != '\0')) {
        ERROR("LRU_K: invalid k value \"%s\"\n", value);
      }
      if (k_val < 1) {
        ERROR("LRU_K: k must be >= 1, got %ld\n", k_val);
      }
      lruk->k = static_cast<int>(k_val);
    } else if (strcasecmp(key, "print") == 0) {
      printf("LRU_K parameters: k=%d\n", lruk->k);
      exit(0);
    } else {
      ERROR("LRU_K does not have parameter %s\n", key);
    }
  }

  free(to_free);
}

/**
 * @brief initialize the cache
 *
 * @param ccache_params some common cache parameters
 * @param cache_specific_params cache specific parameters, e.g. "k=2"
 */
cache_t *LRU_K_init(const common_cache_params_t ccache_params,
                    const char *cache_specific_params) {
  cache_t *cache =
      cache_struct_init("LRU_K", ccache_params, cache_specific_params);
  cache->eviction_params = reinterpret_cast<void *>(new eviction::LRU_K(2));

  cache->cache_init = LRU_K_init;
  cache->cache_free = LRU_K_free;
  cache->get = LRU_K_get;
  cache->find = LRU_K_find;
  cache->insert = LRU_K_insert;
  cache->evict = LRU_K_evict;
  cache->to_evict = LRU_K_to_evict;
  cache->remove = LRU_K_remove;

  LRU_K_parse_params(cache, cache_specific_params);

  if (ccache_params.consider_obj_metadata) {
    auto *lruk = reinterpret_cast<eviction::LRU_K *>(cache->eviction_params);
    /* per-object overhead: K vtimes in history + map entry + queue/set node */
    cache->obj_md_size = 8 * lruk->k + 16;
  } else {
    cache->obj_md_size = 0;
  }

  return cache;
}

/**
 * free resources used by this cache
 *
 * @param cache
 */
static void LRU_K_free(cache_t *cache) {
  delete reinterpret_cast<eviction::LRU_K *>(cache->eviction_params);
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
static bool LRU_K_get(cache_t *cache, const request_t *req) {
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
 * @param update_cache whether to update the cache metadata
 * @return the object or NULL if not found
 */
static cache_obj_t *LRU_K_find(cache_t *cache, const request_t *req,
                                bool update_cache) {
  auto *lruk =
      reinterpret_cast<eviction::LRU_K *>(cache->eviction_params);
  cache_obj_t *obj = cache_find_base(cache, req, update_cache);

  if (obj != nullptr && update_cache) {
    int64_t vtime = cache->n_req;
    auto &hist = lruk->history[obj];

    /* Add the current access to history and maintain window of size K */
    hist.push_back(vtime);
    if ((int)hist.size() > lruk->k) {
      hist.pop_front();
    }

    bool in_fifo = (lruk->fifo_map.count(obj) > 0);

    if (in_fifo && (int)hist.size() >= lruk->k) {
      /* Object has accumulated K accesses: graduate from FIFO to PQ */
      lruk->fifo_queue.erase(lruk->fifo_map[obj]);
      lruk->fifo_map.erase(obj);

      int64_t kth_vtime = hist.front();
      obj_id_t oid = obj->obj_id;
      lruk->pq.insert({kth_vtime, oid});
      lruk->pq_map[obj] = kth_vtime;
    } else if (!in_fifo) {
      /* Object is already in PQ: update its K-th vtime priority */
      int64_t old_kth = lruk->pq_map[obj];
      obj_id_t oid = obj->obj_id;
      lruk->pq.erase({old_kth, oid});
      int64_t new_kth = hist.front();
      lruk->pq.insert({new_kth, oid});
      lruk->pq_map[obj] = new_kth;
    }
    /* else: still in FIFO with < K accesses, no PQ update needed */
  }

  return obj;
}

/**
 * @brief insert an object into the cache.
 * Assumes the cache has enough space; eviction should be performed before
 * calling this function.
 *
 * @param cache
 * @param req
 * @return the inserted object
 */
static cache_obj_t *LRU_K_insert(cache_t *cache, const request_t *req) {
  auto *lruk =
      reinterpret_cast<eviction::LRU_K *>(cache->eviction_params);

  cache_obj_t *obj = cache_insert_base(cache, req);

  int64_t vtime = cache->n_req;
  lruk->history[obj] = {vtime};

  if (lruk->k == 1) {
    /* K=1: every object immediately enters the PQ (equivalent to LRU) */
    obj_id_t oid = obj->obj_id;
    lruk->pq.insert({vtime, oid});
    lruk->pq_map[obj] = vtime;
  } else {
    /* Objects with < K accesses go to the FIFO queue */
    lruk->fifo_queue.push_back(obj);
    lruk->fifo_map[obj] = std::prev(lruk->fifo_queue.end());
  }

  return obj;
}

/**
 * @brief find the object to be evicted without actually evicting it
 *
 * @param cache the cache
 * @return the object to be evicted
 */
static cache_obj_t *LRU_K_to_evict(cache_t *cache, const request_t *req) {
  auto *lruk =
      reinterpret_cast<eviction::LRU_K *>(cache->eviction_params);

  if (!lruk->fifo_queue.empty()) {
    return lruk->fifo_queue.front();
  }
  if (!lruk->pq.empty()) {
    /* pq is ordered by (kth_vtime, obj_id) ascending: the front has the
     * smallest K-th access vtime, which corresponds to the oldest K-th
     * access and therefore the largest backward K-distance */
    obj_id_t evict_id = lruk->pq.begin()->second;
    return hashtable_find_obj_id(cache->hashtable, evict_id);
  }
  return nullptr;
}

/**
 * @brief evict an object from the cache
 *
 * @param cache
 * @param req not used
 */
static void LRU_K_evict(cache_t *cache, const request_t *req) {
  auto *lruk =
      reinterpret_cast<eviction::LRU_K *>(cache->eviction_params);

  cache_obj_t *obj;

  if (!lruk->fifo_queue.empty()) {
    /* Evict from FIFO queue first (infinite backward K-distance) */
    obj = lruk->fifo_queue.front();
    lruk->fifo_queue.pop_front();
    lruk->fifo_map.erase(obj);
  } else {
    /* Evict from PQ: smallest K-th vtime = largest backward K-distance */
    DEBUG_ASSERT(!lruk->pq.empty());
    auto it = lruk->pq.begin();
    obj_id_t evict_id = it->second;
    lruk->pq.erase(it);
    obj = hashtable_find_obj_id(cache->hashtable, evict_id);
    DEBUG_ASSERT(obj != nullptr);
    lruk->pq_map.erase(obj);
  }

  lruk->history.erase(obj);
  cache_remove_obj_base(cache, obj, true);
}

/**
 * @brief remove an object from the cache by user request
 *
 * @param cache
 * @param obj_id
 * @return true if removed, false if not found
 */
static bool LRU_K_remove(cache_t *cache, obj_id_t obj_id) {
  auto *lruk =
      reinterpret_cast<eviction::LRU_K *>(cache->eviction_params);

  cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == nullptr) {
    return false;
  }

  lruk->history.erase(obj);

  auto fifo_it = lruk->fifo_map.find(obj);
  if (fifo_it != lruk->fifo_map.end()) {
    lruk->fifo_queue.erase(fifo_it->second);
    lruk->fifo_map.erase(fifo_it);
  } else {
    auto pq_it = lruk->pq_map.find(obj);
    if (pq_it != lruk->pq_map.end()) {
      int64_t kth_vtime = pq_it->second;
      lruk->pq.erase({kth_vtime, obj_id});
      lruk->pq_map.erase(pq_it);
    }
  }

  cache_remove_obj_base(cache, obj, true);
  return true;
}

#ifdef __cplusplus
}
#endif
