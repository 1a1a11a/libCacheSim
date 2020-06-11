//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef MIMIRCACHE_CACHEOBJ_H
#define MIMIRCACHE_CACHEOBJ_H

#include "../config.h"
#include "request.h"
#include "struct.h"
#include <gmodule.h>


#define print_cache_obj_num(cache_obj) \
    printf("cache_obj id %llu, size %llu, exp_time %llu\n", (unsigned long long) (cache_obj)->obj_id_int, \
           (unsigned long long) (cache_obj)->obj_size, (unsigned long long) (cache_obj)->exp_time)

#define print_cache_obj(cache_obj) print_cache_obj_num(cache_obj)


/* the user needs to make sure that make sure cache_obj is not head */
static inline cache_obj_t* prev_obj_in_slist(cache_obj_t* head, cache_obj_t *cache_obj){
  while (head != NULL && head->list_next != cache_obj)
    head = head->list_next;
  return head;
}

//static inline bool remove_obj_from_slist(cache_obj_t** head, cache_obj_t* cache_obj){
//  if (cache_obj == *head){
//    *head = cache_obj->list_next;
//    return true;
//  }
//
//  cache_obj_t* prev_obj = prev_obj_in_slist(*head, cache_obj);
//  if (prev_obj == NULL)
//    return false;
//  prev_obj->list_next = cache_obj->list_next;
//  return true;
//}

static inline void remove_obj_from_list(cache_obj_t** head, cache_obj_t **tail, cache_obj_t* cache_obj){
//  if (){}
  if (cache_obj == *head){
    *head = cache_obj->list_next;
    cache_obj->list_next->list_prev = NULL;
    return;
  }
  if (cache_obj == *tail){
    *tail = cache_obj->list_prev;
    cache_obj->list_prev->list_next = NULL;
    return;
  }

  cache_obj->list_prev->list_next = cache_obj->list_next;
  cache_obj->list_next->list_prev = cache_obj->list_prev;
  cache_obj->list_prev = NULL;
  cache_obj->list_next = NULL;
  return;
}

static inline void move_obj_to_tail(cache_obj_t** head, cache_obj_t **tail, cache_obj_t* cache_obj){
//  if (){}
  if (cache_obj == *head){
    // change head
    *head = cache_obj->list_next;
    cache_obj->list_next->list_prev = NULL;

    // move to tail
    (*tail)->list_next = cache_obj;
    cache_obj->list_next = NULL;
    cache_obj->list_prev = *tail;
    *tail = cache_obj;
    return;
  }
  if (cache_obj == *tail){
    return;
  }

  // bridge prev and next
  cache_obj->list_prev->list_next = cache_obj->list_next;
  cache_obj->list_next->list_prev = cache_obj->list_prev;

  // handle current tail
  (*tail)->list_next = cache_obj;

  // handle this moving object
  cache_obj->list_next = NULL;
  cache_obj->list_prev = *tail;

  // handle tail
  *tail = cache_obj;
  return;
}


static inline void copy_request_to_cache_obj(cache_obj_t* cache_obj, request_t* req) {
  cache_obj->obj_size = req->obj_size;
#ifdef SUPPORT_TTL
  if (req->ttl != 0)
    cache_obj->exp_time = req->real_time + req->ttl;
#endif
  cache_obj->obj_id_int = req->obj_id_int;
}


static inline cache_obj_t* create_cache_obj_from_request(request_t* req){
  cache_obj_t* cache_obj = my_malloc(cache_obj_t);
  memset(cache_obj, 0, sizeof(cache_obj_t));
  if (req != NULL)
    copy_request_to_cache_obj(cache_obj, req);
  return cache_obj;
}

//static inline void cache_obj_destroyer(gpointer data) {
//#if defined(USE_GLIB_SLICE_ALLOCATOR)
//  g_slice_free(cache_obj_t, data);
//#else
//  g_free(data);
//#endif
//}


static inline void free_cache_obj(cache_obj_t *cache_obj){
  my_free(sizeof(cache_obj_t), cache_obj);
}

/**
 * slab based algorithm related
 */


static inline slab_cache_obj_t* create_slab_cache_obj_from_req(request_t* req){
#ifdef USE_CUSTOME_MEM_ALLOCATOR
  slab_cache_obj_t *cache_obj = new_cache_obj();
#elif defined(USE_GLIB_SLICE_ALLOCATOR)
  slab_cache_obj_t* cache_obj = g_slice_new0(slab_cache_obj_t);
#else
  slab_cache_obj_t *cache_obj = g_new0(slab_cache_obj_t, 1);
#endif
  cache_obj->obj_size = req->obj_size;
#ifdef SUPPORT_TTL
  if (req->ttl != 0)
    cache_obj->exp_time = req->real_time + req->ttl;
  else
    cache_obj->exp_time = G_MAXINT32;
#endif
#ifdef SUPPORT_SLAB_AUTOMOVE
  cache_obj->access_time = req->real_time;
#endif
  cache_obj->obj_id_int = req->obj_id_int;
  return cache_obj;
}


static inline void slab_cache_obj_destroyer(gpointer data){
  slab_cache_obj_t *cache_obj = (slab_cache_obj_t*) data;

#ifdef USE_CUSTOME_MEM_ALLOCATOR
  free_cache_obj(cache_obj);
#elif defined(USE_GLIB_SLICE_ALLOCATOR)
  g_slice_free(slab_cache_obj_t, cache_obj);
#else
  g_free(data);
#endif
}


#endif //MIMIRCACHE_CACHEOBJ_H
