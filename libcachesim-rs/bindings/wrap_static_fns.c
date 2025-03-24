#include "libCacheSim.h"

// Static wrappers

void log_header__extern(int level, const char *file, int line) { log_header(level, file, line); }
__uint16_t _OSSwapInt16__extern(__uint16_t _data) { return _OSSwapInt16(_data); }
__uint32_t _OSSwapInt32__extern(__uint32_t _data) { return _OSSwapInt32(_data); }
__uint64_t _OSSwapInt64__extern(__uint64_t _data) { return _OSSwapInt64(_data); }
request_t * new_request__extern(void) { return new_request(); }
void copy_request__extern(request_t *req_dest, const request_t *req_src) { copy_request(req_dest, req_src); }
request_t * clone_request__extern(const request_t *req) { return clone_request(req); }
void free_request__extern(request_t *req) { free_request(req); }
void print_request__extern(const request_t *req) { print_request(req); }
admissioner_t * create_admissioner__extern(const char *admission_algo, const char *admission_params) { return create_admissioner(admission_algo, admission_params); }
cache_obj_t * prev_obj_in_slist__extern(cache_obj_t *head, cache_obj_t *cache_obj) { return prev_obj_in_slist(head, cache_obj); }
void free_cache_obj__extern(cache_obj_t *cache_obj) { free_cache_obj(cache_obj); }
common_cache_params_t default_common_cache_params__extern(void) { return default_common_cache_params(); }
int64_t cache_get_occupied_byte_default__extern(const cache_t *cache) { return cache_get_occupied_byte_default(cache); }
int64_t cache_get_n_obj_default__extern(const cache_t *cache) { return cache_get_n_obj_default(cache); }
int64_t cache_get_reference_time__extern(const cache_t *cache) { return cache_get_reference_time(cache); }
int64_t cache_get_logical_time__extern(const cache_t *cache) { return cache_get_logical_time(cache); }
int64_t cache_get_virtual_time__extern(const cache_t *cache) { return cache_get_virtual_time(cache); }
void print_cache_stat__extern(const cache_t *cache) { print_cache_stat(cache); }
void record_log2_eviction_age__extern(cache_t *cache, const unsigned long long age) { record_log2_eviction_age(cache, age); }
void record_eviction_age__extern(cache_t *cache, cache_obj_t *obj, const int64_t age) { record_eviction_age(cache, obj, age); }
void generate_cache_name__extern(cache_t *cache, char *str_dest) { generate_cache_name(cache, str_dest); }
void print_sampler__extern(sampler_t *sampler) { print_sampler(sampler); }
void set_default_reader_init_params__extern(reader_init_param_t *params) { set_default_reader_init_params(params); }
reader_init_param_t default_reader_init_params__extern(void) { return default_reader_init_params(); }
reader_t * open_trace__extern(const char *path, const trace_type_e type, const reader_init_param_t *reader_init_param) { return open_trace(path, type, reader_init_param); }
trace_type_e get_trace_type__extern(const const reader_t *const reader) { return get_trace_type(reader); }
bool obj_id_is_num__extern(const const reader_t *const reader) { return obj_id_is_num(reader); }
int read_trace__extern(const reader_t *const reader, const request_t *const req) { return read_trace(reader, req); }
int close_trace__extern(const reader_t *const reader) { return close_trace(reader); }
void print_reader__extern(reader_t *reader) { print_reader(reader); }
