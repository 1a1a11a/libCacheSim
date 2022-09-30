//
// evict the least recent accessed slab
//
// Created by Juncheng Yang on 04/22/20.
//

#include "../include/libCacheSim/evictionAlgo/slabLRU.h"
#include "../utils/include/utilsInternal.h"
#include "../dataStructure/hashtable/hashtable.h"
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

cache_t *slabLRU_init(common_cache_params_t ccache_params, void *init_params) {
  cache_t *cache = cache_struct_init("slabLRU", ccache_params);
  cache->cache_init = slabLRU_init;
  cache->cache_free = slabLRU_free;
  cache->get = slabLRU_get;
  cache->check = slabLRU_check;
  cache->insert = slabLRU_insert;
  cache->evict = slabLRU_evict;
  cache->remove = NULL;

  cache->eviction_params = g_new0(slabLRU_params_t, 1);
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *)(cache->eviction_params);
  slabLRU_params->hashtable =
      g_hash_table_new_full(g_int64_hash, g_direct_equal, NULL, NULL);
  slab_params_t *slab_params = &slabLRU_params->slab_params;
  slab_params->global_slab_q = g_queue_new();
  slab_params->slab_size = MiB;
  slab_params->n_total_slabs =
      ccache_params.cache_size / slab_params->slab_size;
  slab_params->per_obj_metadata_size = 0;

  if (init_params != NULL) {
    slab_init_params_t *slab_init_params = init_params;
    cache->init_params = init_params;
    slab_params->slab_size = slab_init_params->slab_size;
    slab_params->per_obj_metadata_size =
        slab_init_params->per_obj_metadata_size;
  }

  for (int i = 0; i < N_SLABCLASS; i++)
    init_slabclass(&slabLRU_params->slab_params, i);

  VERBOSE("create slabLRU cache size %" PRIu64 ", occupied size %" PRIu64
          ", slab size %" PRIu64 ", per obj metadata %" PRIu8 "\n",
          cache->cache_size, cache->occupied_size, slab_params->slab_size,
          slab_params->per_obj_metadata_size);
  return cache;
}

void slabLRU_free(cache_t *cache) {
  VERBOSE("free slabLRU cache size %" PRIu64 ", occupied size %" PRIu64 "\n",
          cache->cache_size, cache->occupied_size);
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *)(cache->eviction_params);
  slab_params_t *slab_params = &slabLRU_params->slab_params;
  g_queue_free_full(slab_params->global_slab_q, _slab_destroyer);
  for (int i = 0; i < N_SLABCLASS; i++) {
    slabclass_t *slabclass = &slab_params->slabclasses[i];
    g_queue_free(slabclass->free_slab_q);
    g_queue_free(slabclass->obj_q);
  }

  g_hash_table_destroy(slabLRU_params->hashtable);
  cache_struct_free(cache);
}

cache_ck_res_e slabLRU_check(cache_t *cache, request_t *req,
                             bool update_cache) {
  slabLRU_params_t *params = (slabLRU_params_t *)(cache->eviction_params);
  cache_ck_res_e result = cache_ck_miss;
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t *)g_hash_table_lookup(
      params->hashtable, GSIZE_TO_POINTER(req->obj_id));
  if (cache_obj != NULL) {
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
    if (cache_obj->exp_time < req->real_time) {
      result = cache_ck_expired;
      cache_obj->exp_time = req->real_time + req->ttl;
    } else {
      result = cache_ck_hit;
    }
#else
    result = cache_ck_hit;
#endif
    if (update_cache) {
      slab_t *slab = (slab_t *)cache_obj->slab;
      g_queue_unlink(params->slab_params.global_slab_q, slab->q_node);
      g_queue_push_tail_link(params->slab_params.global_slab_q, slab->q_node);
    }
  }
  return result;
}

cache_ck_res_e slabLRU_get(cache_t *cache, request_t *req) {
  VVVERBOSE("slabLRU_get req %" PRIu64 ", real time %" PRIu64
            ", obj id %" PRIu64 ", size %" PRIu32 ", op %d, ttl %" PRIu32 "\n",
            cache->req_cnt, req->real_time, req->obj_id, req->obj_size,
            req->op, req->ttl);
  cache_ck_res_e cache_check = slabLRU_check(cache, req, true);

  /* no update, and eviction is done during insert */
  if (cache_check == cache_ck_miss)
    slabLRU_insert(cache, req);
  else
    slabLRU_update(cache, req);

  return cache_check;
}

void slabLRU_insert(cache_t *cache, request_t *req) {
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *)(cache->eviction_params);
  cache->occupied_size += req->obj_size;
  slab_cache_obj_t *cache_obj = create_slab_cache_obj_from_req(req);
  add_to_slabclass(cache, req, cache_obj, &slabLRU_params->slab_params,
                   slabLRU_evict, NULL);
  g_hash_table_insert(slabLRU_params->hashtable,
                      GSIZE_TO_POINTER(req->obj_id), (gpointer)cache_obj);
}

void slabLRU_update(cache_t *cache, request_t *req) {
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *)(cache->eviction_params);
  slab_cache_obj_t *cache_obj = g_hash_table_lookup(
      slabLRU_params->hashtable, GSIZE_TO_POINTER(req->obj_id));
  slab_t *slab = (slab_t *)cache_obj->slab;
  g_queue_unlink(slabLRU_params->slab_params.global_slab_q, slab->q_node);
  g_queue_push_tail_link(slabLRU_params->slab_params.global_slab_q,
                         slab->q_node);
}

void slabLRU_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
  UNUSED(evicted_obj);
  // evict the most recent accessed slab
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *)(cache->eviction_params);
  slabLRU_params->slab_params.n_allocated_slabs -= 1;
  slab_t *slab_to_evict =
      g_queue_pop_head(slabLRU_params->slab_params.global_slab_q);
  slabclass_t *slabclass =
      &slabLRU_params->slab_params.slabclasses[slab_to_evict->slabclass_id];

  // if current queue is in the free_slab, remove it
  g_queue_remove(slabclass->free_slab_q, slab_to_evict);
  slabclass->n_stored_items -= slab_to_evict->n_stored_items;
  VVERBOSE("evict one slab, current size %" PRIu64 "/%" PRIu64 "\n",
           cache->occupied_size, cache->cache_size);
  for (guint32 i = 0; i < slab_to_evict->n_stored_items; i++) {
    slab_cache_obj_t *cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(
        slabLRU_params->hashtable,
        GSIZE_TO_POINTER(slab_to_evict->slab_items[i]));
    cache->occupied_size -= cache_obj->obj_size;
    g_hash_table_remove(slabLRU_params->hashtable,
                        GSIZE_TO_POINTER(cache_obj->obj_id));
    //    cache_obj_destroyer((gpointer)cache_obj);
  }
  _slab_destroyer(slab_to_evict);
}

void slabLRU_remove_obj(cache_t *cache, cache_obj_t *obj_to_remove) {
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *)(cache->eviction_params);
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t *)g_hash_table_lookup(
      slabLRU_params->hashtable, GSIZE_TO_POINTER(obj_to_remove->obj_id));
  if (cache_obj == NULL) {
    ERROR("obj to remove is not in the cache\n");
    abort();
  }

  cache->occupied_size -= cache_obj->obj_size;
  slab_t *slab = cache_obj->slab;
  // move last item to this pos
  slab->slab_items[cache_obj->item_pos_in_slab] =
      slab->slab_items[slab->n_stored_items - 1];
  // update item_pos_in_slab
  slab_cache_obj_t *last_cache_obj = (slab_cache_obj_t *)g_hash_table_lookup(
      slabLRU_params->hashtable,
      GSIZE_TO_POINTER(slab->slab_items[cache_obj->item_pos_in_slab]));
  last_cache_obj->item_pos_in_slab = cache_obj->item_pos_in_slab;
  slab->n_stored_items -= 1;

  slabclass_t *slabclass =
      &slabLRU_params->slab_params
           .slabclasses[find_slabclass_id(cache_obj->obj_size)];
  slabclass->n_stored_items -= 1;
  g_queue_push_tail(slabclass->free_slab_q, slab);

  free_slab_cache_obj(cache_obj);
}

GHashTable *slabLRU_get_objmap(cache_t *cache) {
  slabLRU_params_t *slabLRU_params = (slabLRU_params_t *)(cache->eviction_params);
  return slabLRU_params->hashtable;
}

#ifdef __cplusplus
}
#endif
