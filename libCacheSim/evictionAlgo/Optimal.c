//
//  a Optimal module that supports different obj size
//
//
//  Optimal.c
//  libCacheSim
//
//
// Created by Juncheng Yang on 3/30/21.
//

#ifdef __cplusplus
extern "C" {
#endif

#include "../dataStructure/hashtable/hashtable.h"
#include "../include/libCacheSim/evictionAlgo/Optimal.h"
#include <assert.h>

/******************* priority queue structs and def **********************/

static int cmp_pri(pqueue_pri_t next, pqueue_pri_t curr) { return (next.pri1 < curr.pri1); }

static pqueue_pri_t get_pri(void *a) { return ((pq_node_t *) a)->pri; }

static void set_pri(void *a, pqueue_pri_t pri) { ((pq_node_t *) a)->pri = pri; }

static size_t get_pos(void *a) { return ((pq_node_t *) a)->pos; }

static void set_pos(void *a, size_t pos) { ((pq_node_t *) a)->pos = pos; }

void *setup_mmap(char *file_path, size_t *size) {
  int fd;
  struct stat st;
  void *mapped_file;

  // set up mmap region
  if ((fd = open(file_path, O_RDONLY)) < 0) {
    ERROR("Unable to open '%s', %s\n", file_path, strerror(errno));
    exit(1);
  }

  if ((fstat(fd, &st)) < 0) {
    close(fd);
    ERROR("Unable to fstat '%s', %s\n", file_path, strerror(errno));
    exit(1);
  }

  *size = st.st_size;
  mapped_file = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
#ifdef MADV_HUGEPAGE
  madvise(mapped_file, st.st_size, MADV_HUGEPAGE | MADV_SEQUENTIAL);
#endif

  if ((mapped_file) == MAP_FAILED) {
    close(fd);
    mapped_file = NULL;
    ERROR("Unable to allocate %llu bytes of memory, %s\n", (unsigned long long) st.st_size,
          strerror(errno));
    abort();
  }

  close(fd);
  return mapped_file;
}


cache_t *Optimal_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("Optimal", ccache_params);
  //  INFO("running belady requires generating a temporary trace with future knowledge, the trace "
  //       "will be stored in the current directory, make sure you have enough space\n");

  Optimal_params_t *params = my_malloc(Optimal_params_t);
  cache->cache_params = params;

  params->pq = pqueue_init((unsigned long) 8e6, cmp_pri, get_pri, set_pri, get_pos, set_pos);
  return cache;
}

void Optimal_free(cache_t *cache) {
  Optimal_params_t *params = cache->cache_params;
  pq_node_t *node = pqueue_pop(params->pq);
  while (node) {
    my_free(sizeof(pq_node_t), node);
    node = pqueue_pop(params->pq);
  }
  pqueue_free(params->pq);

  cache_struct_free(cache);
}

cache_ck_res_e Optimal_check(cache_t *cache, request_t *req, bool update_cache) {
  Optimal_params_t *params = cache->cache_params;
  cache_obj_t *cached_obj;
  cache_ck_res_e ret = cache_check(cache, req, update_cache, &cached_obj);

  if (ret == cache_ck_miss) {
    DEBUG_ASSERT(cache_get_obj(cache, req) == NULL);
  }

  if (!update_cache) return ret;

  if (ret == cache_ck_hit) {
    /* update next access ts, we use INT64_MAX - 10 because we reserve the largest elements for immediate delete */
    if (req->next_access_ts == -1 || req->next_access_ts == INT64_MAX) {
      Optimal_remove_obj(cache, cached_obj);
    } else {
      pqueue_pri_t pri = {.pri1 = req->next_access_ts};
      pqueue_change_priority(params->pq, pri, (pq_node_t *) (cached_obj->extra_metadata_ptr));
      DEBUG_ASSERT(((pq_node_t *) cache_get_obj(cache, req)->extra_metadata_ptr)->pri.pri1
                   == req->next_access_ts);
    }
    return cache_ck_hit;
  } else if (ret == cache_ck_expired) {
    Optimal_remove_obj(cache, cached_obj);

    return cache_ck_miss;
  }

  return cache_ck_miss;
}

cache_ck_res_e Optimal_get(cache_t *cache, request_t *req) {
  DEBUG_ASSERT(req->n_req - 1 == cache->req_cnt);
  /* -2 means the trace does not have next_access ts information */
  DEBUG_ASSERT(req->next_access_ts != -2);
  Optimal_params_t *params = cache->cache_params;

  DEBUG_ASSERT(cache->n_obj == params->pq->size - 1);
  cache_ck_res_e ret = cache_get(cache, req);

  return ret;
}

void Optimal_insert(cache_t *cache, request_t *req) {
  Optimal_params_t *params = cache->cache_params;

  if (req->next_access_ts == -1) {
    return;
  }

  DEBUG_ASSERT(cache_get_obj(cache, req) == NULL);
  cache_obj_t *cached_obj = cache_insert_LRU(cache, req);

  pq_node_t *node = my_malloc(pq_node_t);
  node->obj_id = req->obj_id_int;
  node->pri.pri1 = req->next_access_ts;
  pqueue_insert(params->pq, (void *) node);
  cached_obj->extra_metadata_ptr = node;

  DEBUG_ASSERT(((pq_node_t *) cache_get_obj(cache, req)->extra_metadata_ptr)->pri.pri1
               == req->next_access_ts);
}

void Optimal_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  Optimal_params_t *params = cache->cache_params;
  pq_node_t *node = (pq_node_t *) pqueue_pop(params->pq);

  cache_obj_t *cached_obj = cache_get_obj_by_id(cache, node->obj_id);
  assert(node == cached_obj->extra_metadata_ptr);

#ifdef TRACK_EVICTION_AGE
  record_eviction_age(cache, (int) (req->real_time - cached_obj->last_access_rtime));
#endif

  cached_obj->extra_metadata_ptr = NULL;
  my_free(sizeof(pq_node_t), node);

  Optimal_remove_obj(cache, cached_obj);
}

void Optimal_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  Optimal_params_t *params = cache->cache_params;

  DEBUG_ASSERT(hashtable_find_obj(cache->hashtable, obj_to_remove) == obj_to_remove);

  DEBUG_ASSERT(cache->occupied_size >= obj_to_remove->obj_size);
  cache->occupied_size -= (obj_to_remove->obj_size + cache->per_obj_overhead);

  if (obj_to_remove->extra_metadata_ptr != NULL) {
    /* if it is NULL, it means we have deleted the entry in pq before this */
    pqueue_remove(params->pq, obj_to_remove->extra_metadata_ptr);
    my_free(sizeof(pq_node_t), obj_to_remove->extra_metadata_ptr);
    obj_to_remove->extra_metadata_ptr = NULL;
  }

  remove_obj_from_list(&cache->list_head, &cache->list_tail, obj_to_remove);
  hashtable_delete(cache->hashtable, obj_to_remove);

  cache->n_obj -= 1;
}

void Optimal_remove(cache_t *cache, obj_id_t obj_id) {
  abort();
  Optimal_params_t *params = cache->cache_params;

  cache_obj_t *cache_obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (cache_obj == NULL) {
    WARNING("obj is not in the cache\n");
    return;
  }
  remove_obj_from_list(&cache->list_head, &cache->list_tail, cache_obj);

  assert(cache->occupied_size >= cache_obj->obj_size);
  cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_overhead);

  hashtable_delete(cache->hashtable, cache_obj);
}

#ifdef __cplusplus
}
#endif
