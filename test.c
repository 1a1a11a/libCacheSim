#include <stdio.h>
#include <stdlib.h>

#include <libCacheSim.h>


int main(int argc, char *argv[]) {
  /* open trace, see quickstart.md for opening csv and binary trace */
  reader_t *reader = open_trace("../data/trace.vscsi", VSCSI_TRACE, NULL);

  /* create a container for reading from trace */
  request_t *req = new_request();

  /* create a LRU cache */
  common_cache_params_t cc_params = default_common_cache_params();
  cc_params.cache_size = 1024 * 1024U;
  cache_t *cache = LRU_init(cc_params, NULL);

  /* counters */
  uint64_t req_byte = 0, miss_byte = 0;

  /* loop through the trace */
  while (read_one_req(reader, req) == 0) {
    if (cache->get(cache, req) == false) {
      miss_byte += req->obj_size;
    }
    req_byte += req->obj_size;
  }

  /* cleaning */
  close_trace(reader);
  free_request(req);
  cache->cache_free(cache);

  return 0;
}

// compile with the following
// gcc test.c $(pkg-config --cflags --libs libCacheSim glib-2.0) -o test.out
