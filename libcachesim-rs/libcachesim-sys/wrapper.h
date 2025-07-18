// Wrapper header for bindgen to generate Rust FFI bindings
// This includes the main libCacheSim headers we need to bind

#include <glib.h>
#include <stdbool.h>
#include <stdint.h>

// Include config.h first to get obj_id_t definition
#include <config.h>

// Core libCacheSim headers - include in specific order to avoid conflicts
#include <libCacheSim/cache.h>
#include <libCacheSim/cacheObj.h>
#include <libCacheSim/const.h>
#include <libCacheSim/enum.h>
#include <libCacheSim/reader.h>
#include <libCacheSim/request.h>

// Helper functions for request management since static inline functions
// are not bindable - we'll implement these in a separate C file
request_t *libcachesim_new_request(void);
void libcachesim_free_request(request_t *req);
void libcachesim_copy_request(request_t *req_dest, const request_t *req_src);
request_t *libcachesim_clone_request(const request_t *req);

// Cache parameter helpers
common_cache_params_t libcachesim_default_common_cache_params(void);

// Reader parameter helpers
reader_init_param_t libcachesim_default_reader_init_params(void);
void libcachesim_set_default_reader_init_params(reader_init_param_t *params);

// Cache statistics helpers
int64_t libcachesim_cache_get_occupied_byte_default(const cache_t *cache);
int64_t libcachesim_cache_get_n_obj_default(const cache_t *cache);
int64_t libcachesim_cache_get_reference_time(const cache_t *cache);
