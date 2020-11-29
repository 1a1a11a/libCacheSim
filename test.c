#include <libCacheSim.h>

int main(int argc, char *argv[])
{
  /* open trace, see quickstart.md for opening csv and binary trace */
  reader_t *reader = open_trace("data/trace.vscsi", VSCSI_TRACE, OBJ_ID_NUM, NULL);

  /* craete a container for reading from trace */
  request_t *req = new_request();

  /* create a LRU cache */
  common_cache_params_t cc_params = {.cache_size=1024*1024U};
  cache_t *cache = create_cache("LRU", cc_params, NULL);

  /* counters */
  uint64_t req_byte = 0, miss_byte = 0;

  /* loop through the trace */
  while (read_one_req(reader, req) == 0) {
  if (cache->get(cache, req) == cache_ck_miss) {
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