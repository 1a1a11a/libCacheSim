//
//  myLRU.hpp
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef myLRU_h
#define myLRU_h


#include <iostream>
#include <cstdlib>
#include <string>
#include <cstdio>
#include <iterator>
#include <memory>
#include <unordered_map>
#include <list>
//#include "libCacheSim.h"
#include "../../libCacheSim/include/libCacheSim.h"


namespace std {

  typedef std::list<CacheObj>::iterator ListIter;


  class MyLRUCpp {
  public:
    size_t size;
    size_t used_size;
    uint64_t logical_time;

    unordered_map <CacheObj, ListIter> hashtable;
    list <CacheObj> lru_queue;

    explicit MyLRUCpp(size_t size): size(size), used_size(0), logical_time(0) {}

    ~MyLRUCpp() = default;;

    bool check(request_t *req);

    bool add(request_t *req);

    void insert(request_t *req);

    void update(request_t *req);

    void evict(request_t *req);

    CacheObj* evict_with_return(request_t *req);

    bool remove(request_t *req);

    size_t get_size() {return used_size; }

  };
}

extern "C" cache_t *myLRU_init(guint64 size, obj_id_type_t obj_id_type, guint64 block_size, void *params);

// I need to re-implement LRU completely in CPP then wrap with libCacheSim API



#endif  /* myLRU_HPP */
