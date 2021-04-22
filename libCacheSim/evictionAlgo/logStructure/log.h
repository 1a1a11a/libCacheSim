#pragma once

//#include

#include "../../include/libCacheSim/evictionAlgo/LLSC.h"

#include "../../dataStructure/hashtable/hashtable.h"


//static inline void add_to_seg(cache_t *cache, segment_t *segment, cache_obj_t *cache_obj, bool is_training_seg) {
//  segment->n_total_obj += 1;
//  segment->total_byte += cache_obj->obj_size + cache->per_obj_overhead;
//  cache_obj->ref_cnt += 1;
//
//  if (segment->last_obj == NULL) {
//    segment->first_obj = cache_obj;
//  } else {
//    segment->last_obj->list_next = cache_obj;
//  }
//  segment->last_obj = cache_obj;
//
//  /*
//   * an object can exist in two seg, data seg, or training seg
//   * we use list_prev_void to point to its original seg,
//   * extra_metadata_ptr2 to point to the training seg
//   */
//  if (is_training_seg) {
//    cache_obj->list_prev_void = segment;
//  } else {
//    cache_obj->segment = segment;
//  }
//}

static inline int cmp_double(const void *p1, const void *p2) {
  if (*(double *) p1 < *(double *) p2)
    return -1;
  else
    return 1;
}



static inline int LLSC_find_bucket_idx(request_t *req) {
  return 0;
}


static inline bool is_seg_evictable_fifo(segment_t *seg, int min_evictable) {
  if (seg == NULL)
    return false;

  int n_evictable = 0;
  while (seg->next_seg != NULL && n_evictable < min_evictable) {
    n_evictable += 1;
    seg = seg->next_seg;
  }
  return n_evictable == min_evictable;
}


static inline bool obj_in_hashtable(cache_t *cache, cache_obj_t *cache_obj) {
  cache_obj_t *obj = hashtable_find_obj(cache->hashtable, cache_obj);
  bool find = false;
  while (obj != NULL) {
    if (obj == cache_obj) {
      find = true;
      break;
    }
    obj = obj->hash_next;
  }
  return find;
}

static inline void clean_one_seg(cache_t *cache, segment_t *seg) {
  LLSC_params_t *params = cache->cache_params;

  cache_obj_t *cache_obj;

  DEBUG_ASSERT(seg->n_total_obj == params->segment_size);
  for (int i = 0; i < seg->n_total_obj; i++) {
    cache_obj = &seg->objs[i];
    hashtable_delete(cache->hashtable, cache_obj);
  }
  my_free(sizeof(cache_obj_t) * params->n_total_obj, seg->objs);
  my_free(sizeof(segment_t), seg);
}


static inline void remove_seg_from_bucket(bucket_t *bucket, segment_t *segment) {
  if (bucket->first_seg == segment) {
    bucket->first_seg = segment->next_seg;
  }
  if (bucket->last_seg == segment) {
    bucket->last_seg = segment->prev_seg;
  }

  if (segment->prev_seg != NULL) {
    segment->prev_seg->next_seg = segment->next_seg;
  }

  if (segment->next_seg != NULL) {
    segment->next_seg->prev_seg = segment->prev_seg;
  }
}

static inline void append_seg_to_bucket(bucket_t *bucket, segment_t *segment) {
  if (bucket->last_seg == NULL) {
    DEBUG_ASSERT(bucket->first_seg == NULL);
    bucket->first_seg = segment;
  } else {
    bucket->last_seg->next_seg = segment;
    segment->prev_seg = bucket->last_seg;
  }

  bucket->last_seg = segment;
  segment->next_seg = NULL;
}

static inline segment_t *allocate_new_seg(cache_t *cache, int32_t create_time) {
  LLSC_params_t *params = cache->cache_params;

  /* allocate a new segment */
  segment_t *new_seg = my_malloc(segment_t);
  memset(new_seg, 0, sizeof(segment_t));
  new_seg->objs = my_malloc_n(cache_obj_t, params->segment_size);
  DEBUG_ASSERT(new_seg->objs != NULL);
  memset(new_seg->objs, 0, sizeof(cache_obj_t) * params->segment_size);

  new_seg->next_seg = NULL;
  //  new_seg->first_obj = new_seg->last_obj = NULL;
  new_seg->n_total_obj = new_seg->total_byte = 0;
  new_seg->create_time = create_time;
  new_seg->is_training_seg = false;
  new_seg->magic = MAGIC;
  new_seg->seg_id = params->n_allocated_segs++;

  return new_seg;
}

static inline void link_new_seg_before_seg(bucket_t *bucket, segment_t *old_seg, segment_t *new_seg) {
  if (old_seg->prev_seg == NULL) {
    DEBUG_ASSERT(bucket->first_seg == old_seg);
    bucket->first_seg = new_seg;
  } else {
    old_seg->prev_seg->next_seg = new_seg;
  }
  new_seg->prev_seg = old_seg->prev_seg;
  new_seg->next_seg = old_seg;
  old_seg->prev_seg = new_seg;
}

