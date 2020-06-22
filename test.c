
#include <libCacheSim.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  uint64_t req_cnt = 0, req_byte = 0;
  uint64_t miss_cnt = 0, miss_byte = 0;
  reader_t *reader =
      setup_reader("data/trace.vscsi", VSCSI_TRACE, OBJ_ID_NUM, NULL);
  request_t *req = new_request();
  read_one_req(reader_t * reader, request_t * req);
  while (req->valid) {
    req_cnt++;
    req_byte += req->obj_size;
    if (cache->get(cache, req) != cache_hit_e) {
      miss_cnt++;
      miss_byte += req->obj_size;
    }
    read_one_req(cloned_reader, req);
  }
}

// gcc test.c -o test -lglib2.0 -ltcmalloc


cache API: 
    cache_t *cache_init(common_cache_params_t, void*);
    void cache_free(cache_t *);
    cache_check_result_t get(cache_t *, request_t *);
    cache_check_result_t check(cache_t *, request_t *, bool );
    void _insert(cache_t *, request_t *);
    void _evict(cache_t *, request_t *, cache_obj_t *);
    void remove_obj(cache_t *, request *);




myfunc_inclusive() {
    if (cache1->get(cache1, req) == cache_miss_e){
        // L1 cache miss 
        cache2->_insert(cache2, req); // also place in L2
        // L2 eviction needs back invalidation 
        if (cache2->used_size > cache2->cache_size){
            cache2->_evict(cache2, req, evicted_obj);
            cache1->remove_obj(cache1, evicted_obj);
        }
    } else {
        // L1 cache hit 
    } 
}

myfunc_exclusive() {
    if (cache1->get(cache1, req) == cache_miss_e){
        // L1 cache miss 
        if (cache2->check(cache2, req, true) == cache_miss_e){
            // L2 cache miss, do nothing to L2 
        } else {
            // L2 cache hit, move to L1; 
            cache2->remove_obj(cache2, req);
        }  
        // L2 eviction needs back invalidation 
        if (cache2->used_size > cache2->cache_size){
            cache2->_evict(cache2, req, evicted_obj);
            cache1->remove_obj(cache1, evicted_obj);
        }
    }; 
}
