////
//// Created by Juncheng Yang on 4/11/21.
////
//
//#include "../include/libCacheSim/evictionAlgo/segcache.h"
//#include "include/cacheAlg.h"
//
//
//#ifdef __cplusplus
//extern "C" {
//#endif
//
//
//cache_t *SEGCACHE_init(common_cache_params_t ccache_params, void *init_params) {
//  cache_t *cache = cache_struct_init("SEGCACHE", ccache_params);
//  segcache_init_params_t *segcache_init_params = init_params;
//  segcache_params_t *segcache_params = my_malloc(segcache_params_t);
//
//  segcache_params->n_merge = segcache_init_params->n_merge;
//  segcache_params->segment_size = segcache_init_params->segment_size;
//  segcache_params->min_mature_time = segcache_init_params->min_mature_time;
//  segcache_params->n_segment = 0;
//
//  segcache_params->first_seg = NULL;
//  segcache_params->last_seg = NULL;
//  segcache_params->free_seg = NULL;
//  segcache_params->next_seg_to_merge = NULL;
//
//  cache->cache_params = segcache_params;
//  return cache;
//}
//
//void SEGCACHE_free(cache_t *cache) {
//  segcache_params_t *params = cache->cache_params;
//  segment_t *seg = params->first_seg;
//  segment_t *next_seg;
//
//  while (seg != NULL) {
//    next_seg = seg->next_seg;
//    my_free(sizeof(segment_t), seg);
//    seg = next_seg;
//    params->n_segment --;
//  }
//
//  seg = params->free_seg;
//  while (seg != NULL) {
//    next_seg = seg->next_seg;
//    my_free(sizeof(segment_t), seg);
//    seg = next_seg;
//    params->n_segment --;
//  }
//
//  assert(params->n_segment == 0);
//
//  cache_struct_free(cache);
//}
//
//
//
//
//cache_ck_res_e SEGCACHE_check(cache_t *cache, request_t *req, bool update_cache) {
//  cache_obj_t *cache_obj;
//  cache_ck_res_e ret = cache_check(cache, req, update_cache, &cache_obj);
//
//  if (cache_obj && likely(update_cache)) {
//    cache_obj->freq += 1;
//  }
//  return ret;
//}
//
//cache_ck_res_e SEGCACHE_get(cache_t *cache, request_t *req) {
//  return cache_get(cache, req);
//}
//
//void SEGCACHE_insert(cache_t *cache, request_t *req) {
//#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
//  if (cache->default_ttl != 0 && req->ttl == 0) {
//    req->ttl = cache->default_ttl;
//  }
//#endif
//
//  cache_obj_t *cache_obj = hashtable_insert(cache->hashtable, req);
//  cache->occupied_size += cache_obj->obj_size + cache->per_obj_overhead;
//  cache->n_obj += 1;
//
//
//  segcache_params_t *params = cache->cache_params;
//  segment_t *seg = params->last_seg;
//  if (seg->total_byte + req->obj_size + cache->per_obj_overhead <= params->segment_size) {
//    /* allocate a new segment */
//    segment_t *new_seg = my_malloc(segment_t);
//    memset(new_seg, 0, sizeof(segment_t));
//    seg->last_obj->list_next = NULL;
//    seg->next_seg = new_seg;
//    new_seg->prev_seg = seg;
//    params->last_seg = new_seg;
//    params->n_segment += 1;
//    seg = new_seg;
//    seg->first_obj = cache_obj;
//  }
//
//  seg->last_obj->list_next = cache_obj;
//  cache_obj->list_prev = seg->last_obj;
//  seg->last_obj = cache_obj;
//  seg->total_byte += req->obj_size + cache->per_obj_overhead;
//  seg->n_obj += 1;
//}
//
//
//static inline bool has_enough_seg(segment_t* seg, int n_seg, int64_t max_creation_time) {
//  int n = 0;
//  while (n < n_seg && seg != NULL && seg->creation_time < max_creation_time) {
//    n += 1;
//    seg = seg->next_seg;
//  }
//
//  return n == n_seg;
//}
//
//int cmp_obj_info(const void *p1, const void *p2) {
//  const struct obj_info *o1 = p1;
//  const struct obj_info *o2 = p2;
//
//  if (o1->score < o2->score)
//    return 1;
//  else
//    return -1;
//}
//
//void SEGCACHE_evict(cache_t *cache, request_t *req, cache_obj_t *evicted_obj) {
//  segcache_params_t *params = cache->cache_params;
//
//  segment_t *start_seg = params->next_seg_to_merge;
//  /* merge n_merge segments into one */
//  if (start_seg == NULL || !has_enough_seg(start_seg, params->n_merge, req->real_time - params->min_mature_time))
//    start_seg = params->first_seg;
//
//  segment_t *curr_seg = start_seg;
//  int32_t n_obj = 0;
//  for (int i = 0; i < params->n_merge; i++) {
//    n_obj += curr_seg->n_obj;
//    curr_seg = curr_seg->next_seg;
//  }
//
//  static struct obj_info *obj_info_array = NULL;
//  static int32_t obj_info_array_len = 0;
//  if (obj_info_array == NULL || obj_info_array_len < n_obj) {
//    if (obj_info_array != NULL)
//      my_free(sizeof(struct obj_info) * score_array_len, obj_info_array);
//    obj_info_array = my_malloc_n(struct obj_info, n_obj);
//  }
//
//  int n_seg = 0;
//  int pos = 0;
//  cache_obj_t *cache_obj = curr_seg->first_obj;
//  while (n_seg < params->n_merge) {
//    obj_info_array[pos].score = (double) (cache_obj->freq + 0.01) / cache_obj->obj_size;
//    obj_info_array[pos++].size = cache_obj->obj_size;
//    curr_seg = curr_seg->next_seg;
//    n_seg += 1;
//  }
//  DEBUG_ASSERT(pos == n_obj);
//  qsort(obj_info_array, pos, sizeof(double), cmp_obj_info);
//
//  int64_t accu_size = 0;
//  pos = 0;
//  while (pos < n_obj && accu_size < params->segment_size) {
//    accu_size += obj_info_array[pos++].size;
//  }
//  double cutoff = obj_info_array[pos].score;
//
//
//  cache_obj = curr_seg->first_obj;
//  n_seg = 0;
//  int64_t retained_size = 0;
//  segment_t *new_seg = my_malloc(segment_t);
//  cache_obj_t *last_retained_obj = NULL, *next_obj;
//  while (n_seg < params->n_merge && retained_size < params->segment_size - 8) {
//    while (cache_obj != NULL) {
//      next_obj = cache_obj->list_next;
//      if ((double) (cache_obj->freq + 0.01) / (cache_obj->obj_size) > cutoff && retained_size < params->segment_size) {
//        /* keep the object */
//        if (last_retained_obj == NULL) {
//          new_seg->first_obj = cache_obj;
//        } else {
//          last_retained_obj->list_next = cache_obj;
//        }
//        cache_obj->list_prev = last_retained_obj;
//        last_retained_obj = cache_obj;
//        retained_size += cache_obj->obj_size + cache->per_obj_overhead;
//      } else {
//        hashtable_delete(cache->hashtable, cache_obj);
//        cache->occupied_size -= (cache_obj->obj_size + cache->per_obj_overhead);
//        cache->n_obj -= 1;
//
//      }
//      cache_obj = next_obj;
//    } /* end of segment */
//      curr_seg = curr_seg->next_seg;
//      n_seg += 1;
//      params->n_segment -= 1;
//      my_free(sizeof(segment_t), curr_seg->prev_seg);
//      curr_seg->prev_seg = NULL;
//  }
//  last_retained_obj->list_next = NULL;
//
//
//
//
//
//
//
//
//
//
//  cache_obj_t *obj_to_evict = cache->list_head;
//  if (evicted_obj != NULL) {
//    // return evicted object to caller
//    memcpy(evicted_obj, obj_to_evict, sizeof(cache_obj_t));
//  }
//  DEBUG_ASSERT(cache->list_head != cache->list_head->list_next);
//  cache->list_head = cache->list_head->list_next;
//  cache->list_head->list_prev = NULL;
//  DEBUG_ASSERT(cache->occupied_size >= obj_to_evict->obj_size);
//  cache->occupied_size -= (obj_to_evict->obj_size + cache->per_obj_overhead);
//  cache->n_obj -= 1;
//
//  hashtable_delete(cache->hashtable, obj_to_evict);
//  DEBUG_ASSERT(cache->list_head != cache->list_head->list_next);
//
//
//  cache_evict_LRU(cache, req, evicted_obj);
//}
//
//void SEGCACHE_remove(cache_t *cache, obj_id_t obj_id) {
//  segcache_params_t *params = cache->cache_params;
//
//
//  cache_obj_t *cache_obj = hashtable_find_obj_id(cache->hashtable, obj_id);
//  if (cache_obj == NULL) {
//    WARNING("obj to remove is not in the cache\n");
//    return;
//  }
//  remove_obj_from_list(&cache->list_head, &cache->list_tail, cache_obj);
//  assert(cache->occupied_size >= cache_obj->obj_size);
//  cache->occupied_size -= cache_obj->obj_size;
//
//  hashtable_delete(cache->hashtable, cache_obj);
//}
//
//
//#ifdef __cplusplus
//extern "C" {
//#endif
