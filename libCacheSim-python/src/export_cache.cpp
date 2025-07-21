// libcachesim_python - libCacheSim Python bindings
// Export cache core functions and classes
// Copyright 2025 The libcachesim Authors.  All rights reserved.
//
// Use of this source code is governed by a GPL-3.0
// license that can be found in the LICENSE file or at
// https://github.com/1a1a11a/libcachesim/blob/develop/LICENSE

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <iostream>
#include <memory>
#include <sstream>

#include "config.h"
#include "dataStructure/hashtable/hashtable.h"
#include "export.h"
#include "libCacheSim/cache.h"
#include "libCacheSim/cacheObj.h"
#include "libCacheSim/enum.h"
#include "libCacheSim/evictionAlgo.h"
#include "libCacheSim/plugin.h"
#include "libCacheSim/request.h"

namespace libcachesim {

namespace py = pybind11;

// Custom deleters for smart pointers
struct CacheDeleter {
  void operator()(cache_t* ptr) const {
    if (ptr != nullptr) ptr->cache_free(ptr);
  }
};

struct CommonCacheParamsDeleter {
  void operator()(common_cache_params_t* ptr) const {
    if (ptr != nullptr) {
      delete ptr;  // Simple delete for POD struct
    }
  }
};

struct CacheObjectDeleter {
  void operator()(cache_obj_t* ptr) const {
    if (ptr != nullptr) free_cache_obj(ptr);
  }
};

struct RequestDeleter {
  void operator()(request_t* ptr) const {
    if (ptr != nullptr) free_request(ptr);
  }
};

// ***********************************************************************
// ****             Python plugin cache implementation BEGIN          ****
// ***********************************************************************

// Forward declaration with appropriate visibility
struct pypluginCache_params;

typedef struct __attribute__((visibility("hidden"))) pypluginCache_params {
  py::object data;  ///< Plugin's internal data structure (python object)
  py::function cache_init_hook;
  py::function cache_hit_hook;
  py::function cache_miss_hook;
  py::function cache_eviction_hook;
  py::function cache_remove_hook;
  py::function cache_free_hook;
  std::string cache_name;
} pypluginCache_params_t;

// Custom deleter for pypluginCache_params_t
struct PypluginCacheParamsDeleter {
  void operator()(pypluginCache_params_t* ptr) const {
    if (ptr != nullptr) {
      // Call the free hook if available before deletion
      if (!ptr->cache_free_hook.is_none()) {
        try {
          ptr->cache_free_hook(ptr->data);
        } catch (...) {
          // Ignore exceptions during cleanup to prevent double-fault
        }
      }
      delete ptr;
    }
  }
};

static void pypluginCache_free(cache_t* cache);
static bool pypluginCache_get(cache_t* cache, const request_t* req);
static cache_obj_t* pypluginCache_find(cache_t* cache, const request_t* req,
                                       const bool update_cache);
static cache_obj_t* pypluginCache_insert(cache_t* cache, const request_t* req);
static cache_obj_t* pypluginCache_to_evict(cache_t* cache,
                                           const request_t* req);
static void pypluginCache_evict(cache_t* cache, const request_t* req);
static bool pypluginCache_remove(cache_t* cache, const obj_id_t obj_id);

cache_t* pypluginCache_init(
    const common_cache_params_t ccache_params, std::string cache_name,
    py::function cache_init_hook, py::function cache_hit_hook,
    py::function cache_miss_hook, py::function cache_eviction_hook,
    py::function cache_remove_hook, py::function cache_free_hook) {
  // Initialize base cache structure with exception safety
  cache_t* cache = nullptr;
  std::unique_ptr<pypluginCache_params_t, PypluginCacheParamsDeleter> params;

  try {
    cache = cache_struct_init(cache_name.c_str(), ccache_params, NULL);
    if (!cache) {
      throw std::runtime_error("Failed to initialize cache structure");
    }

    // Set function pointers for cache operations
    cache->cache_init = NULL;
    cache->cache_free = pypluginCache_free;
    cache->get = pypluginCache_get;
    cache->find = pypluginCache_find;
    cache->insert = pypluginCache_insert;
    cache->evict = pypluginCache_evict;
    cache->remove = pypluginCache_remove;
    cache->to_evict = pypluginCache_to_evict;
    cache->get_occupied_byte = cache_get_occupied_byte_default;
    cache->get_n_obj = cache_get_n_obj_default;
    cache->can_insert = cache_can_insert_default;
    cache->obj_md_size = 0;

    // Allocate and initialize plugin parameters using smart pointer with custom
    // deleter
    params =
        std::unique_ptr<pypluginCache_params_t, PypluginCacheParamsDeleter>(
            new pypluginCache_params_t(), PypluginCacheParamsDeleter());
    params->cache_name = cache_name;
    params->cache_init_hook = cache_init_hook;
    params->cache_hit_hook = cache_hit_hook;
    params->cache_miss_hook = cache_miss_hook;
    params->cache_eviction_hook = cache_eviction_hook;
    params->cache_remove_hook = cache_remove_hook;
    params->cache_free_hook = cache_free_hook;

    // Initialize the cache data - this might throw
    params->data = cache_init_hook(ccache_params);

    // Transfer ownership to the cache structure
    cache->eviction_params = params.release();

    return cache;

  } catch (...) {
    // Clean up on exception
    if (cache) {
      cache_struct_free(cache);
    }
    // params will be automatically cleaned up by smart pointer destructor
    throw;  // Re-throw the exception
  }
}

static void pypluginCache_free(cache_t* cache) {
  if (!cache || !cache->eviction_params) {
    return;
  }

  // Use smart pointer for automatic cleanup
  std::unique_ptr<pypluginCache_params_t, PypluginCacheParamsDeleter> params(
      static_cast<pypluginCache_params_t*>(cache->eviction_params));

  // The smart pointer destructor will handle cleanup automatically
  cache_struct_free(cache);
}

static bool pypluginCache_get(cache_t* cache, const request_t* req) {
  bool hit = cache_get_base(cache, req);
  pypluginCache_params_t* params =
      (pypluginCache_params_t*)cache->eviction_params;

  if (hit) {
    params->cache_hit_hook(params->data, req);
  } else {
    params->cache_miss_hook(params->data, req);
  }

  return hit;
}

static cache_obj_t* pypluginCache_find(cache_t* cache, const request_t* req,
                                       const bool update_cache) {
  return cache_find_base(cache, req, update_cache);
}

static cache_obj_t* pypluginCache_insert(cache_t* cache, const request_t* req) {
  return cache_insert_base(cache, req);
}

static cache_obj_t* pypluginCache_to_evict(cache_t* cache,
                                           const request_t* req) {
  throw std::runtime_error("pypluginCache does not support to_evict function");
}

static void pypluginCache_evict(cache_t* cache, const request_t* req) {
  pypluginCache_params_t* params =
      (pypluginCache_params_t*)cache->eviction_params;

  // Get eviction candidate from plugin
  py::object result = params->cache_eviction_hook(params->data, req);
  obj_id_t obj_id = result.cast<obj_id_t>();

  // Find the object in the cache
  cache_obj_t* obj_to_evict = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj_to_evict == NULL) {
    throw std::runtime_error("pypluginCache: object " + std::to_string(obj_id) +
                             " to be evicted not found in cache");
  }

  // Perform the eviction
  cache_evict_base(cache, obj_to_evict, true);
}

static bool pypluginCache_remove(cache_t* cache, const obj_id_t obj_id) {
  pypluginCache_params_t* params =
      (pypluginCache_params_t*)cache->eviction_params;

  // Notify plugin of the removal
  params->cache_remove_hook(params->data, obj_id);

  // Find the object in the cache
  cache_obj_t* obj = hashtable_find_obj_id(cache->hashtable, obj_id);
  if (obj == NULL) {
    return false;
  }

  // Remove the object from the cache
  cache_remove_obj_base(cache, obj, true);
  return true;
}

// ***********************************************************************
// ****            Python plugin cache implementation END             ****
// ***********************************************************************

// Templates
template <cache_t* (*InitFn)(common_cache_params_t, const char*)>
auto make_cache_wrapper(const std::string& fn_name) {
  return [=](py::module_& m) {
    m.def(
        fn_name.c_str(),
        [](const common_cache_params_t& cc_params,
           const std::string& cache_specific_params) {
          const char* params_cstr = cache_specific_params.empty()
                                        ? nullptr
                                        : cache_specific_params.c_str();
          cache_t* ptr = InitFn(cc_params, params_cstr);
          return std::unique_ptr<cache_t, CacheDeleter>(ptr);
        },
        "cc_params"_a, "cache_specific_params"_a = "");
  };
}

void export_cache(py::module& m) {
  /**
   * @brief Cache structure
   */
  py::class_<cache_t, std::unique_ptr<cache_t, CacheDeleter>>(m, "Cache")
      .def_readonly("cache_size", &cache_t::cache_size)
      .def_readonly("default_ttl", &cache_t::default_ttl)
      .def_readonly("obj_md_size", &cache_t::obj_md_size)
      .def_readonly("n_req", &cache_t::n_req)
      .def_readonly("cache_name", &cache_t::cache_name)
      .def_readonly("init_params", &cache_t::init_params)
      .def(
          "get",
          [](cache_t& self, const request_t& req) {
            return self.get(&self, &req);
          },
          "req"_a)
      .def(
          "find",
          [](cache_t& self, const request_t& req, const bool update_cache) {
            return self.find(&self, &req, update_cache);
          },
          "req"_a, "update_cache"_a = true)
      .def(
          "can_insert",
          [](cache_t& self, const request_t& req) {
            return self.can_insert(&self, &req);
          },
          "req"_a)
      .def(
          "insert",
          [](cache_t& self, const request_t& req) {
            return self.insert(&self, &req);
          },
          "req"_a)
      .def(
          "need_eviction",
          [](cache_t& self, const request_t& req) {
            return self.need_eviction(&self, &req);
          },
          "req"_a)
      .def(
          "evict",
          [](cache_t& self, const request_t& req) {
            return self.evict(&self, &req);
          },
          "req"_a)
      .def(
          "remove",
          [](cache_t& self, obj_id_t obj_id) {
            return self.remove(&self, obj_id);
          },
          "obj_id"_a)
      .def(
          "to_evict",
          [](cache_t& self, const request_t& req) {
            return self.to_evict(&self, &req);
          },
          "req"_a)
      .def("get_occupied_byte",
           [](cache_t& self) { return self.get_occupied_byte(&self); })
      .def("get_n_obj", [](cache_t& self) { return self.get_n_obj(&self); })
      .def("print_cache", [](cache_t& self) {
        // Capture stdout to return as string
        std::ostringstream captured_output;
        std::streambuf* orig = std::cout.rdbuf();
        std::cout.rdbuf(captured_output.rdbuf());

        self.print_cache(&self);

        // Restore original stdout
        std::cout.rdbuf(orig);
        return captured_output.str();
      });

  /**
   * @brief Common cache parameters
   */
  py::class_<common_cache_params_t,
             std::unique_ptr<common_cache_params_t, CommonCacheParamsDeleter>>(
      m, "CommonCacheParams")
      .def(py::init([](uint64_t cache_size, uint64_t default_ttl,
                       int32_t hashpower, bool consider_obj_metadata) {
             common_cache_params_t* params = new common_cache_params_t();
             params->cache_size = cache_size;
             params->default_ttl = default_ttl;
             params->hashpower = hashpower;
             params->consider_obj_metadata = consider_obj_metadata;
             return params;
           }),
           "cache_size"_a, "default_ttl"_a = 86400 * 300, "hashpower"_a = 24,
           "consider_obj_metadata"_a = false)
      .def_readwrite("cache_size", &common_cache_params_t::cache_size)
      .def_readwrite("default_ttl", &common_cache_params_t::default_ttl)
      .def_readwrite("hashpower", &common_cache_params_t::hashpower)
      .def_readwrite("consider_obj_metadata",
                     &common_cache_params_t::consider_obj_metadata);

  /**
   * @brief Cache object
   *
   * TODO: full support for cache object
   */
  py::class_<cache_obj_t, std::unique_ptr<cache_obj_t, CacheObjectDeleter>>(
      m, "CacheObject")
      .def_readonly("obj_id", &cache_obj_t::obj_id)
      .def_readonly("obj_size", &cache_obj_t::obj_size);

  /**
   * @brief Request operation enumeration
   */
  py::enum_<req_op_e>(m, "ReqOp")
      .value("OP_NOP", OP_NOP)
      .value("OP_GET", OP_GET)
      .value("OP_GETS", OP_GETS)
      .value("OP_SET", OP_SET)
      .value("OP_ADD", OP_ADD)
      .value("OP_CAS", OP_CAS)
      .value("OP_REPLACE", OP_REPLACE)
      .value("OP_APPEND", OP_APPEND)
      .value("OP_PREPEND", OP_PREPEND)
      .value("OP_DELETE", OP_DELETE)
      .value("OP_INCR", OP_INCR)
      .value("OP_DECR", OP_DECR)
      .value("OP_READ", OP_READ)
      .value("OP_WRITE", OP_WRITE)
      .value("OP_UPDATE", OP_UPDATE)
      .value("OP_INVALID", OP_INVALID)
      .export_values();

  /**
   * @brief Request structure
   */
  py::class_<request_t, std::unique_ptr<request_t, RequestDeleter>>(m,
                                                                    "Request")
      .def(py::init([](int64_t obj_size, req_op_e op, bool valid,
                       obj_id_t obj_id, int64_t clock_time, uint64_t hv,
                       int64_t next_access_vtime, int32_t ttl) {
             request_t* req = new_request();
             req->obj_size = obj_size;
             req->op = op;
             req->valid = valid;
             req->obj_id = obj_id;
             req->clock_time = clock_time;
             req->hv = hv;
             req->next_access_vtime = next_access_vtime;
             req->ttl = ttl;
             return req;
           }),
           "obj_size"_a = 1, "op"_a = OP_NOP, "valid"_a = true, "obj_id"_a = 0,
           "clock_time"_a = 0, "hv"_a = 0, "next_access_vtime"_a = -2,
           "ttl"_a = 0)
      .def_readwrite("clock_time", &request_t::clock_time)
      .def_readwrite("hv", &request_t::hv)
      .def_readwrite("obj_id", &request_t::obj_id)
      .def_readwrite("obj_size", &request_t::obj_size)
      .def_readwrite("ttl", &request_t::ttl)
      .def_readwrite("op", &request_t::op)
      .def_readwrite("valid", &request_t::valid)
      .def_readwrite("next_access_vtime", &request_t::next_access_vtime);

  /**
   * @brief Generic function to create a cache instance.
   *
   * TODO: add support for general cache creation and add support for cache
   * specific parameters this is a backup for cache creation in python.
   */

  // Cache algorithm initialization functions

  make_cache_wrapper<ARC_init>("ARC_init")(m);
  make_cache_wrapper<ARCv0_init>("ARCv0_init")(m);
  make_cache_wrapper<CAR_init>("CAR_init")(m);
  make_cache_wrapper<Cacheus_init>("Cacheus_init")(m);
  make_cache_wrapper<Clock_init>("Clock_init")(m);
  make_cache_wrapper<ClockPro_init>("ClockPro_init")(m);
  make_cache_wrapper<FIFO_init>("FIFO_init")(m);
  make_cache_wrapper<FIFO_Merge_init>("FIFO_Merge_init")(m);
  make_cache_wrapper<flashProb_init>("flashProb_init")(m);
  make_cache_wrapper<GDSF_init>("GDSF_init")(m);
  make_cache_wrapper<LHD_init>("LHD_init")(m);
  make_cache_wrapper<LeCaR_init>("LeCaR_init")(m);
  make_cache_wrapper<LeCaRv0_init>("LeCaRv0_init")(m);
  make_cache_wrapper<LFU_init>("LFU_init")(m);
  make_cache_wrapper<LFUCpp_init>("LFUCpp_init")(m);
  make_cache_wrapper<LFUDA_init>("LFUDA_init")(m);
  make_cache_wrapper<LIRS_init>("LIRS_init")(m);
  make_cache_wrapper<LRU_init>("LRU_init")(m);
  make_cache_wrapper<LRU_Prob_init>("LRU_Prob_init")(m);
  make_cache_wrapper<nop_init>("nop_init")(m);

  make_cache_wrapper<QDLP_init>("QDLP_init")(m);
  make_cache_wrapper<Random_init>("Random_init")(m);
  make_cache_wrapper<RandomLRU_init>("RandomLRU_init")(m);
  make_cache_wrapper<RandomTwo_init>("RandomTwo_init")(m);
  make_cache_wrapper<S3FIFO_init>("S3FIFO_init")(m);
  make_cache_wrapper<S3FIFOv0_init>("S3FIFOv0_init")(m);
  make_cache_wrapper<S3FIFOd_init>("S3FIFOd_init")(m);
  make_cache_wrapper<Sieve_init>("Sieve_init")(m);
  make_cache_wrapper<Size_init>("Size_init")(m);
  make_cache_wrapper<SLRU_init>("SLRU_init")(m);
  make_cache_wrapper<SLRUv0_init>("SLRUv0_init")(m);
  make_cache_wrapper<TwoQ_init>("TwoQ_init")(m);
  make_cache_wrapper<WTinyLFU_init>("WTinyLFU_init")(m);
  make_cache_wrapper<Hyperbolic_init>("Hyperbolic_init")(m);
  make_cache_wrapper<Belady_init>("Belady_init")(m);
  make_cache_wrapper<BeladySize_init>("BeladySize_init")(m);

#ifdef ENABLE_3L_CACHE
  make_cache_wrapper<ThreeLCache_init>("ThreeLCache_init")(m);
#endif

#ifdef ENABLE_GLCACHE
  make_cache_wrapper<GLCache_init>("GLCache_init")(m);
#endif

#ifdef ENABLE_LRB
  make_cache_wrapper<LRB_init>("LRB_init")(m);
#endif

  // ***********************************************************************
  // ****                                                               ****
  // ****               Python plugin cache bindings                   ****
  // ****                                                               ****
  // ***********************************************************************

  m.def("pypluginCache_init", &pypluginCache_init, "cc_params"_a,
        "cache_name"_a, "cache_init_hook"_a, "cache_hit_hook"_a,
        "cache_miss_hook"_a, "cache_eviction_hook"_a, "cache_remove_hook"_a,
        "cache_free_hook"_a);
  // ***********************************************************************
  // ****                                                               ****
  // ****                end functions for python plugin                ****
  // ****                                                               ****
  // ***********************************************************************

  m.def(
      "c_process_trace",
      [](cache_t& cache, reader_t& reader, int64_t start_req = 0,
         int64_t max_req = -1) {
        reset_reader(&reader);
        if (start_req > 0) {
          skip_n_req(&reader, start_req);
        }

        request_t* req = new_request();
        int64_t n_req = 0, n_hit = 0;
        int64_t bytes_req = 0, bytes_hit = 0;
        bool hit;

        read_one_req(&reader, req);
        while (req->valid) {
          n_req += 1;
          bytes_req += req->obj_size;
          hit = cache.get(&cache, req);
          if (hit) {
            n_hit += 1;
            bytes_hit += req->obj_size;
          }
          read_one_req(&reader, req);
          if (max_req > 0 && n_req >= max_req) {
            break;  // Stop if we reached the max request limit
          }
        }

        free_request(req);
        // return the miss ratio
        double obj_miss_ratio = n_req > 0 ? 1.0 - (double)n_hit / n_req : 0.0;
        double byte_miss_ratio =
            bytes_req > 0 ? 1.0 - (double)bytes_hit / bytes_req : 0.0;
        return std::make_tuple(obj_miss_ratio, byte_miss_ratio);
      },
      "cache"_a, "reader"_a, "start_req"_a = 0, "max_req"_a = -1);
}

}  // namespace libcachesim
