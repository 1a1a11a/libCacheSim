// Helper functions for libCacheSim Rust bindings
// These implement static inline functions that can't be bound directly

#include <libCacheSim/cache.h>
#include <libCacheSim/reader.h>
#include <libCacheSim/request.h>

// Request management functions
request_t *libcachesim_new_request(void) { return new_request(); }

void libcachesim_free_request(request_t *req) { free_request(req); }

void libcachesim_copy_request(request_t *req_dest, const request_t *req_src) {
  copy_request(req_dest, req_src);
}

request_t *libcachesim_clone_request(const request_t *req) {
  return clone_request(req);
}

// Cache parameter helpers
common_cache_params_t libcachesim_default_common_cache_params(void) {
  return default_common_cache_params();
}

// Reader parameter helpers
reader_init_param_t libcachesim_default_reader_init_params(void) {
  return default_reader_init_params();
}

void libcachesim_set_default_reader_init_params(reader_init_param_t *params) {
  set_default_reader_init_params(params);
}

// Cache statistics helpers
int64_t libcachesim_cache_get_occupied_byte_default(const cache_t *cache) {
  return cache_get_occupied_byte_default(cache);
}

int64_t libcachesim_cache_get_n_obj_default(const cache_t *cache) {
  return cache_get_n_obj_default(cache);
}

int64_t libcachesim_cache_get_reference_time(const cache_t *cache) {
  return cache_get_reference_time(cache);
}
