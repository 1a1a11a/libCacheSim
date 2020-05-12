//
// Created by Juncheng Yang on 11/15/19.
//

#include "cacheSimulator.h"
// gcc cacheSimulator.c -I ../libMimircache/include/ $(pkg-config --cflags glib-2.0) -ldl -L. -l:libmimircache.so
// gcc cacheSimulator.c -I ../libMimircache/include/ $(pkg-config --cflags glib-2.0) -ldl -L. -llibmimircache $(pkg-config --libs glib-2.0) -ldl -lm


int main(int argc, char **argv){
  reader_init_param_t init_params = {.obj_id_field=6, .has_header=FALSE, .delimiter='\t'};
  reader_t *reader = setup_reader("../../data/trace.csv", CSV_TRACE, OBJ_ID_STR, &init_params);
  request_t* req = new_request(OBJ_ID_STR);
  read_one_req(reader, req);
  for (int i = 0; i < 18 && req->valid; i++) {
    printf("read in %s, time %ld\n", (gchar*) req->obj_id_ptr, (long) req->real_time);
    read_one_req(reader, req);
  }

  common_cache_params_t cc_params = {.cache_size=20, .obj_id_type=OBJ_ID_STR, .support_ttl=FALSE};
  cache_t *lru = create_cache("LRU", cc_params, NULL);
  printf("%d\n", lru->core.get(lru, req));

  cache_t *mylru = create_cache("myLRU", cc_params, NULL);
  printf("%d\n", mylru->core.get(mylru, req));

  free_request(req);
  cache_struct_free(lru);
  cache_struct_free(mylru);

  return 0;
}

