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
  cache_t *cache = cache_struct_init("slabLRC", ccache_params);
  cache->cache_params = my_malloc(slabLRC_params_t);
  memset(cache->cache_params, 0, sizeof(slabLRC_params_t));
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
  slabLRC_params->hashtable = create_hash_table_with_obj_id_type(OBJ_ID_NUM, NULL, free_slab_cache_obj, g_free, free_slab_cache_obj);
  slabLRC_params->slab_params.global_slab_q = g_queue_new();
  slabLRC_params->slab_params.slab_size = MiB;
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
  g_queue_free_full(slab_params->global_slab_q, _slab_destroyer);
  for (int i=0; i < N_SLABCLASS; i++){
    slabclass_t *slabclass = &slab_params->slabclasses[i];
    g_queue_free(slabclass->free_slab_q);
    g_queue_free(slabclass->obj_q);
  }

  g_hash_table_destroy(slabLRC_params->hashtable);
//  my_free(sizeof(slabLRC_params), slabLRC_params);
//  my_free(sizeof(slab_params), slabLRC_params);
  cache_struct_free(cache);
}

cache_check_result_t slabLRC_check(cache_t *cache, request_t *req, bool update_cache){
  slabLRC_params_t *params = (slabLRC_params_t *) (cache->cache_params);
  // slabLRC does not require update

  cache_check_result_t result = cache_miss_e;
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(params->hashtable, GSIZE_TO_POINTER(req->obj_id_int));

  if (cache_obj != NULL) {
#ifdef SUPPORT_TTL
    if (cache_obj->exp_time < req->real_time) {
      /* obj is expired */
      result = expired_e;
      cache_obj->exp_time = req->real_time + req->ttl;
    } else {
      result = cache_hit_e;
    }
#else
    result = cache_hit_e;
#endif
  }

  return result;
}

cache_check_result_t slabLRC_get(cache_t *cache, request_t *req){
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
//  printf("req %d\n", cache->core.req_cnt);
//  print_request(req);
  assert(slabLRC_params->slab_params.n_total_slabs == cache->core.cache_size/MiB);
  cache_check_result_t cache_check = slabLRC_check(cache, req, true);
  assert(slabLRC_params->slab_params.n_total_slabs == cache->core.cache_size/MiB);

  /* no update, and eviction is done during insert */
  if (cache_check == cache_miss_e)
    _slabLRC_insert(cache, req);
  assert(slabLRC_params->slab_params.n_total_slabs == cache->core.cache_size/MiB);

  cache->core.req_cnt += 1;
  return cache_check;

}

void _slabLRC_insert(cache_t *cache, request_t *req){
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
  cache->core.used_size += req->obj_size;
  slab_cache_obj_t* cache_obj = create_slab_cache_obj_from_req(req);
  add_to_slabclass(cache, req, cache_obj, &slabLRC_params->slab_params, _slabLRC_evict, NULL);
  g_hash_table_insert(slabLRC_params->hashtable, GSIZE_TO_POINTER(req->obj_id_int), (gpointer) cache_obj);
}


void _slabLRC_evict(cache_t *cache, request_t *req, cache_obj_t* evicted_obj){
  // evict the most recent created
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
  slabLRC_params->slab_params.n_allocated_slabs -= 1;
  slab_t *slab_to_evict = g_queue_pop_head(slabLRC_params->slab_params.global_slab_q);
  slabclass_t *slabclass = &slabLRC_params->slab_params.slabclasses[slab_to_evict->slabclass_id];
  DEBUG3("evict slab id %d store %u - total %u items\n", slab_to_evict->slabclass_id, (unsigned)slab_to_evict->n_stored_items, (unsigned)slab_to_evict->n_total_items);

  // if current queue is in the free_slab, remove it
  g_queue_remove(slabclass->free_slab_q, slab_to_evict);
  slabclass->n_stored_items -= slab_to_evict->n_stored_items;
  for (guint32 i=0; i<slab_to_evict->n_stored_items; i++){
    slab_cache_obj_t *cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(slabLRC_params->hashtable,slab_to_evict->slab_items[i]);
    g_hash_table_remove(slabLRC_params->hashtable, GSIZE_TO_POINTER(cache_obj->obj_id_int));
//    cache_obj_destroyer((gpointer)cache_obj);
  }
  _slab_destroyer(slab_to_evict);
}

void slabLRC_remove_obj(cache_t *cache, request_t* req){
  slabLRC_params_t *slabLRC_params = (slabLRC_params_t *) (cache->cache_params);
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t *) g_hash_table_lookup(slabLRC_params->hashtable, GSIZE_TO_POINTER(req->obj_id_int));
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

  free_slab_cache_obj(cache_obj);

}


#ifdef __cplusplus
}
#endif
