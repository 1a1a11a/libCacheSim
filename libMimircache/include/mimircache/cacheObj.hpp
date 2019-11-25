//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef MIMIRCACHE_CACHEOBJ_HPP
#define MIMIRCACHE_CACHEOBJ_HPP

#include <glib.h>
#include <string>
#include <iostream>
#include "request.h"


namespace std {
  class CacheObj {
  public:
    // we assume all pointer is at least 64-bit
    void *obj_id;
    obj_id_t obj_id_type;
    uint32_t size;
    void *extra_data;

    CacheObj(request *req) : size(req->size), obj_id_type(req->obj_id_type), extra_data(req->extra_data) {
      if (req->obj_id_type == OBJ_ID_NUM) {
        this->obj_id = req->obj_id_ptr;
      } else if (req->obj_id_type == OBJ_ID_STR) {
        this->obj_id = static_cast<void *> (new std::string(static_cast<char*>(req->obj_id)));
      } else {
        std::cerr << "CacheObj initialization: unknown obj_id_type " << req->obj_id_type << " in request" << std::endl;
        abort();
      }
    }

    CacheObj(const CacheObj& cache_obj) : size(cache_obj.size), obj_id_type(cache_obj.obj_id_type), extra_data(NULL) {
      if (cache_obj.obj_id_type == OBJ_ID_NUM) {
        this->obj_id = cache_obj.obj_id;
      } else if (cache_obj.obj_id_type == OBJ_ID_STR) {
        this->obj_id = static_cast<void *> (new std::string(static_cast<char*>(cache_obj.obj_id)));
      } else {
        std::cerr << "unknown obj_id_type " << cache_obj.obj_id_type << " in object to copy" << std::endl;
        abort();
      }
    }

    ~CacheObj() {
      if (obj_id_type == OBJ_ID_STR) {
        delete obj_id;
      }
      if (extra_data != NULL) {
        // this is annoying, extra data is allocated by others, and we need to make sure it is allocated using malloc
        free(extra_data);
      }
    }

    bool operator==(const CacheObj &cache_obj) const {
      if (obj_id_type == OBJ_ID_NUM) {
        return obj_id == cache_obj.obj_id;
      } else if (obj_id_type == OBJ_ID_STR) {
        return (*static_cast<std::string*>(obj_id)).compare(*static_cast<std::string*>(cache_obj.obj_id));
      } else {
        std::cerr << "CacheObj comparison: unknown obj_id_type " << obj_id_type << " in cache_obj" << std::endl;
        abort();
      }
    }
  };

  template <> struct hash<CacheObj> {
    inline size_t operator()(const CacheObj &cache_obj) const {
      size_t seed = 0;
      if (cache_obj.obj_id_type == OBJ_ID_NUM) {
        std::hash<uint64_t> hasher;
        seed = hasher(reinterpret_cast<guint64>(cache_obj.obj_id)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      } else {
        std::hash<string> hasher;
//        std::string s = *static_cast<string *>(cache_obj.obj_id);
        seed = hasher(*static_cast<string *>(cache_obj.obj_id)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
//        std::cout << "key " << *static_cast<string *>(cache_obj.obj_id) << ", seed " << seed << std::endl;
      }
      return seed;
    }
  };


// adapted from Daniel https://github.com/dasebe/webcachesim/blob/master/caches/cache_object.h
// hash_combine derived from boost/functional/hash/hash.hpp:212
// Copyright 2005-2014 Daniel James.
// Distributed under the Boost Software License, Version 1.0.
// (See http://www.boost.org/LICENSE_1_0.txt)
  template<class T>
  inline void hash_combine(std::size_t &seed, const T &v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
}

#endif //MIMIRCACHE_CACHEOBJ_HPP
