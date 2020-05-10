//
//  myLRU.cpp
//  libMimircache
//
//  Created by Juncheng on 11/16/19.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "myLRU.hpp"

using namespace std;

inline bool MyLRUCpp::check(request_t *req) {
  CacheObj cache_obj(req);
  return hashtable.find(cache_obj) != hashtable.end();
}

bool MyLRUCpp::add(request_t *req) {
  bool exist = check(req);
  if (req->obj_size <= size) {
    if (exist)
      update(req);
    else
      insert(req);
    while (used_size > size)
      evict(req);
  } else {
      clog << "obj size " << req->obj_size << " larger than cache size " << size << endl;
  }
  logical_time++;
  return exist;
}

void MyLRUCpp::insert(request_t *req) {
  CacheObj cache_obj(req);
  lru_queue.emplace_front(cache_obj);
  hashtable[cache_obj] = lru_queue.begin();
//  std::cout << "insert " << *static_cast<string*>(cache_obj.obj_id) << " - type " << cache_obj.obj_id_type << ", " \
//    << " - size " << lru_queue.size() << ", size " << hashtable.size() << std::endl;
  used_size += req->size;
}

void MyLRUCpp::update(request_t *req) {
  CacheObj cache_obj(req);
  auto it = hashtable.find(cache_obj);
  used_size = used_size - it->first.size + req->obj_size;
  lru_queue.erase(it->second);
  hashtable.erase(cache_obj);
  lru_queue.emplace_front(cache_obj);
  hashtable[cache_obj] = lru_queue.begin();
//  lru_queue.splice(lru_queue.begin(), lru_queue, it->second);
//  it->first.size = req->size; // update obj size
}

void MyLRUCpp::evict(request_t *req) {
  CacheObj cache_obj = lru_queue.back();
  used_size -= cache_obj.size;
  hashtable.erase(cache_obj);
  lru_queue.pop_back();
}

CacheObj* MyLRUCpp::evict_with_return(request_t *req) {
  CacheObj cache_obj = lru_queue.back();
  CacheObj *ret_obj = new CacheObj(cache_obj);
  used_size -= cache_obj.size;
  hashtable.erase(cache_obj);
  lru_queue.pop_back();
  return ret_obj;
}

bool MyLRUCpp::remove(request_t *req) {
  ;
}


void __myLRU_insert_element(cache_t *cache, request_t *req) {
  MyLRUCpp *myLRU = static_cast<MyLRUCpp *>(cache->cache_params);
  myLRU->insert(req);
  cache->core.used_size = myLRU->used_size;
}

gboolean myLRU_check_element(cache_t *cache, request_t *req) {
  MyLRUCpp *myLRU = static_cast<MyLRUCpp *>(cache->cache_params);
  return (gboolean) (myLRU->check(req));
}

void __myLRU_update_element(cache_t *cache, request_t *req) {
  MyLRUCpp *myLRU = static_cast<MyLRUCpp *>(cache->cache_params);
  myLRU->update(req);
  cache->core.used_size = myLRU->used_size;
}

void __myLRU_evict_element(cache_t *cache, request_t *req) {
  MyLRUCpp *myLRU = static_cast<MyLRUCpp *>(cache->cache_params);
  myLRU->evict(req);
  cache->core.used_size = myLRU->used_size;
}

gpointer __myLRU__evict_with_return(cache_t *cache, request_t *req) {
  /** evict one element and return the evicted element,
   * needs to free the memory of returned data
   */

  cerr << __func__ << " is not used in most of places, so we can skip the implementation until necessary" << endl;
  MyLRUCpp *myLRU = static_cast<MyLRUCpp *>(cache->cache_params);
  CacheObj *cache_obj = myLRU->evict_with_return(req);
  cache->core.used_size = myLRU->used_size;

  if (req->obj_id_type == OBJ_ID_NUM) {
    uint64_t *evicted_key = g_new(uint64_t, 1);
    *evicted_key = reinterpret_cast<uint64_t> (cache_obj->obj_id);
    return static_cast<gpointer>(evicted_key);
  } else {
    return static_cast<gpointer>(g_strdup((gchar *) cache_obj->obj_id));
  }
}

gboolean myLRU_add_element(cache_t *cache, request_t *req) {
  MyLRUCpp *myLRU = static_cast<MyLRUCpp *>(cache->cache_params);
  gboolean exist = static_cast<gboolean>(myLRU->add(req));
  cache->core.used_size = myLRU->used_size;
  return exist;
}


void myLRU_destroy(cache_t *cache) {
  MyLRUCpp *myLRU = static_cast<MyLRUCpp *>(cache->cache_params);
  delete myLRU;
  cache_struct_free(cache);
}

void myLRU_destroy_unique(cache_t *cache) {
  /* the difference between destroy_unique and destroy
   is that the former one only free the resources that are
   unique to the cache, freeing these resources won't affect
   other caches copied from original cache
   in Optimal, next_access should not be freed in destroy_unique,
   because it is shared between different caches copied from the original one.
   */
  /* move next_access to reader */

  myLRU_destroy(cache);
}


uint64_t myLRU_get_size(cache_t *cache) {
  MyLRUCpp *myLRU = static_cast<MyLRUCpp *>(cache->cache_params);
  return static_cast<uint64_t>(myLRU->get_size());
}


cache_t *myLRU_init(uint64_t size, obj_id_type_t obj_id_type, uint64_t block_size, void *params) {
  cache_t *cache = cache_struct_init("myLRU", size, obj_id_type);
  cache->cache_params = static_cast<void *>(new MyLRUCpp(size));
  return cache;
}


void myLRU_remove_element(cache_t *cache, void *data_to_remove) {
  MyLRUCpp *myLRU = static_cast<MyLRUCpp *>(cache->cache_params);

  ;
}


