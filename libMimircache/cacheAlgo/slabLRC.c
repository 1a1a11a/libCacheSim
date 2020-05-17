//
// evict the least recent created slab
//
// Created by Juncheng Yang on 04/22/20.
//

#include "slabLRC.h"
#include "assert.h"


#ifdef __cplusplus
extern "C" {
#endif


cache_t *slabLRC_init(common_cache_params_t ccache_params, void *cache_specific_init_params) {
//  if (ccache_params.cache_size < MB * N_SLABCLASS){
//    ERROR("slab based algorithm needs size at least %d * %ld, but given %llu\n", N_SLABCLASS, MB, (unsigned long long) ccache_params.cache_size);
//    abort();
//  }
  cache_t *cache = cache_struct_init("slabLRC", ccache_params);
  cache->cache_params = g_new0(slabLRC_params_t, 1);
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
  slabLRC_params->hashtable = create_hash_table_with_obj_id_type(ccache_params.obj_id_type, NULL, slab_cache_obj_destroyer, g_free, slab_cache_obj_destroyer);
  slabLRC_params->slab_params.slab_q = g_queue_new();
  slabLRC_params->slab_params.slab_size = MB;
  slabLRC_params->slab_params.n_total_slabs = ccache_params.cache_size / slabLRC_params->slab_params.slab_size;
  slabLRC_params->slab_params.per_obj_metadata_size = 0;

  if (cache_specific_init_params != NULL){
    slab_init_params_t *init_params = cache_specific_init_params;
    cache->core.cache_specific_init_params = cache_specific_init_params;
    slabLRC_params->slab_params.slab_size = init_params->slab_size;
    slabLRC_params->slab_params.per_obj_metadata_size = init_params->per_obj_metadata_size;
  }

  for (int i=0; i<N_SLABCLASS; i++)
    init_slabclass(&slabLRC_params->slab_params, i);

  return cache;
}

void slabLRC_free(cache_t *cache){
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
  slab_params_t *slab_params = &slabLRC_params->slab_params;
  g_queue_free_full(slab_params->slab_q, _slab_destroyer);
  for (int i=0; i < N_SLABCLASS; i++){
    slabclass_t *slabclass = &slab_params->slabclasses[i];
    g_queue_free(slabclass->free_slab_q);
    g_queue_free(slabclass->obj_q);
  }

  g_hash_table_destroy(slabLRC_params->hashtable);
  cache_struct_free(cache);
}

gboolean slabLRC_check(cache_t *cache, request_t *req){
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
  return g_hash_table_contains(slabLRC_params->hashtable, req->obj_id_ptr);
}

gboolean slabLRC_get(cache_t *cache, request_t *req){
  gboolean found_in_cache = slabLRC_check(cache, req);

  /* no update, and eviction is done during insert */
  if (!found_in_cache)
    _slabLRC_insert(cache, req);

  cache->core.req_cnt += 1;
  return found_in_cache;

}

void _slabLRC_insert(cache_t *cache, request_t *req){
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
  cache->core.used_size += req->obj_size;
  slab_cache_obj_t* cache_obj = create_slab_cache_obj_from_req(req);
  add_to_slabclass(cache, req, cache_obj, &slabLRC_params->slab_params, _slabLRC_evict, NULL);
  g_hash_table_insert(slabLRC_params->hashtable, cache_obj->obj_id_ptr, (gpointer) cache_obj);
}

void _slabLRC_update(cache_t *cache, request_t *req){
  ;
}

void _slabLRC_evict(cache_t *cache, request_t *req){
  // evict the most recent created
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
  slabLRC_params->slab_params.n_allocated_slabs -= 1;
  slab_t *slab_to_evict = g_queue_pop_head(slabLRC_params->slab_params.slab_q);
  slabclass_t *slabclass = &slabLRC_params->slab_params.slabclasses[slab_to_evict->slabclass_id];
  DEBUG3("evict slab id %d store %u - total %u items\n", slab_to_evict->slabclass_id, (unsigned)slab_to_evict->n_stored_items, (unsigned)slab_to_evict->n_total_items);

  // if current queue is in the free_slab, remove it
  g_queue_remove(slabclass->free_slab_q, slab_to_evict);
  slabclass->n_stored_items -= slab_to_evict->n_stored_items;
  for (guint32 i=0; i<slab_to_evict->n_stored_items; i++){
    slab_cache_obj_t *cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(slabLRC_params->hashtable, slab_to_evict->slab_items[i]);
    g_hash_table_remove(slabLRC_params->hashtable, cache_obj->obj_id_ptr);
//    cache_obj_destroyer((gpointer)cache_obj);
  }
  _slab_destroyer(slab_to_evict);
}

gpointer slabLRC_evict_with_return(cache_t *cache, request_t *req){
  ERROR("not suppoerted\n");
  return NULL;
}


void slabLRC_remove_obj(cache_t *cache, void *obj_id_to_remove){
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(slabLRC_params->hashtable, obj_id_to_remove);
  if (cache_obj == NULL) {
    ERROR("obj to remove is not in the cache\n");
    abort();
  }

  cache->core.used_size -= cache_obj->obj_size;
  slab_t *slab = cache_obj->slab;
  // move last item to this pos
  slab->slab_items[cache_obj->item_pos_in_slab] = slab->slab_items[slab->n_stored_items-1];
  // update item_pos_in_slab
  slab_cache_obj_t *last_cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(slabLRC_params->hashtable, slab->slab_items[cache_obj->item_pos_in_slab]);
  last_cache_obj->item_pos_in_slab = cache_obj->item_pos_in_slab;
  slab->n_stored_items -=1;

  slabclass_t *slabclass = &slabLRC_params->slab_params.slabclasses[find_slabclass_id(cache_obj->obj_size)];
  slabclass->n_stored_items -= 1;
  g_queue_push_tail(slabclass->free_slab_q, slab);

  destroy_cache_obj(cache_obj);

}

GHashTable *slabLRC_get_objmap(cache_t *cache){
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
  return slabLRC_params->hashtable;
}




/**************************** TTL support **************************/
cache_check_result_t slabLRC_check_and_update_with_ttl(cache_t *cache, request_t* req){
  slabLRC_params_t *params = (slabLRC_params_t *) (cache->cache_params);
  cache_check_result_t result = cache_miss_e;
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(params->hashtable, req->obj_id_ptr);
  if (cache_obj != NULL) {
    if (cache_obj->exp_time < req->real_time) {
      /* obj is expired */
      result = expired_e;
      cache->stat.hit_expired_cnt += 1;
      cache->stat.hit_expired_byte += cache_obj->obj_size;
      cache_obj->exp_time = req->real_time + req->ttl;
    } else {
      result = cache_hit_e;
    }
  }
  return result;
}

gboolean slabLRC_get_with_ttl(cache_t *cache, request_t *req){
  req->ttl == 0 && (req->ttl = cache->core.default_ttl);
  cache_check_result_t result = slabLRC_check_and_update_with_ttl(cache, req);
  gboolean found_in_cache = (result == cache_hit_e);

  /* no update, and eviction is done during insert */
  if (result == cache_miss_e){
    _slabLRC_insert(cache, req);
  }

  cache->core.req_cnt += 1;
  return found_in_cache;

}


#ifdef __cplusplus
}
#endif
