

#include "../include/libCacheSim/cacheObj.h"

#include <assert.h>
#include <gmodule.h>

#include "../include/libCacheSim/request.h"

/**
 * copy the cache_obj to req_dest
 * @param req_dest
 * @param cache_obj
 */
void copy_cache_obj_to_request(request_t *req_dest,
                               const cache_obj_t *cache_obj) {
  req_dest->obj_id = cache_obj->obj_id;
  req_dest->obj_size = cache_obj->obj_size;
  req_dest->valid = true;
}

/**
 * copy the data from request into cache_obj
 * @param cache_obj
 * @param req
 */
void copy_request_to_cache_obj(cache_obj_t *cache_obj, const request_t *req) {
  cache_obj->obj_size = req->obj_size;
#if defined(SUPPORT_TTL) && SUPPORT_TTL == 1
  if (req->ttl != 0)
    cache_obj->exp_time = req->real_time + req->ttl;
  else
    cache_obj->exp_time = UINT32_MAX;
#endif
  cache_obj->obj_id = req->obj_id;
}

/**
 * create a cache_obj from request
 * @param req
 * @return
 */
cache_obj_t *create_cache_obj_from_request(const request_t *req) {
  cache_obj_t *cache_obj = my_malloc(cache_obj_t);
  memset(cache_obj, 0, sizeof(cache_obj_t));
  if (req != NULL) copy_request_to_cache_obj(cache_obj, req);
  return cache_obj;
}

/** remove the object from the built-in doubly linked list
 *
 * @param head
 * @param tail
 * @param cache_obj
 */
void remove_obj_from_list(cache_obj_t **head, cache_obj_t **tail,
                          cache_obj_t *cache_obj) {
  if (cache_obj == *head) {
    *head = cache_obj->queue.next;
    if (cache_obj->queue.next != NULL) cache_obj->queue.next->queue.prev = NULL;
  }
  if (cache_obj == *tail) {
    *tail = cache_obj->queue.prev;
    if (cache_obj->queue.prev != NULL) cache_obj->queue.prev->queue.next = NULL;
  }

  if (cache_obj->queue.prev != NULL)
    cache_obj->queue.prev->queue.next = cache_obj->queue.next;

  if (cache_obj->queue.next != NULL)
    cache_obj->queue.next->queue.prev = cache_obj->queue.prev;

  cache_obj->queue.prev = NULL;
  cache_obj->queue.next = NULL;
}

/**
 * move an object to the tail of the doubly linked list
 * @param head
 * @param tail
 * @param cache_obj
 */
void move_obj_to_tail(cache_obj_t **head, cache_obj_t **tail,
                      cache_obj_t *cache_obj) {
  if (*head == *tail) {
    // the list only has one element
    assert(cache_obj == *head);
    assert(cache_obj->queue.next == NULL);
    assert(cache_obj->queue.prev == NULL);
    return;
  }
  if (cache_obj == *head) {
    // change head
    *head = cache_obj->queue.next;
    cache_obj->queue.next->queue.prev = NULL;

    // move to tail
    (*tail)->queue.next = cache_obj;
    cache_obj->queue.next = NULL;
    cache_obj->queue.prev = *tail;
    *tail = cache_obj;
    return;
  }
  if (cache_obj == *tail) {
    return;
  }

  // bridge list_prev and next
  cache_obj->queue.prev->queue.next = cache_obj->queue.next;
  cache_obj->queue.next->queue.prev = cache_obj->queue.prev;

  // handle current tail
  (*tail)->queue.next = cache_obj;

  // handle this moving object
  cache_obj->queue.next = NULL;
  cache_obj->queue.prev = *tail;

  // handle tail
  *tail = cache_obj;
}

/**
 * move an object to the head of the doubly linked list
 * @param head
 * @param tail
 * @param cache_obj
 */
void move_obj_to_head(cache_obj_t **head, cache_obj_t **tail,
                      cache_obj_t *cache_obj) {
  if (*head == *tail) {
    // the list only has one element
    assert(cache_obj == *head);
    assert(cache_obj->queue.next == NULL);
    assert(cache_obj->queue.prev == NULL);
    return;
  }
  if (cache_obj == *head) {
    return;
  }
  if (cache_obj == *tail) {
    // change tail
    *tail = cache_obj->queue.prev;
    cache_obj->queue.prev->queue.next = NULL;

    // move to head
    (*head)->queue.prev = cache_obj;
    cache_obj->queue.prev = NULL;
    cache_obj->queue.next = *head;
    *head = cache_obj;
    return;
  }

  // bridge list_prev and next
  cache_obj->queue.prev->queue.next = cache_obj->queue.next;
  cache_obj->queue.next->queue.prev = cache_obj->queue.prev;

  // handle current head
  (*head)->queue.prev = cache_obj;

  // handle this moving object
  cache_obj->queue.prev = NULL;
  cache_obj->queue.next = *head;

  // handle head
  *head = cache_obj;
}
