//
// Created by Juncheng Yang on 11/15/19.
//

#include "cacheSimulator.h"
// gcc cacheSimulator.c -I ../CMimircache/include/ $(pkg-config --cflags glib-2.0) -ldl -L. -l:libmimircache.so
// gcc cacheSimulator.c -I ../CMimircache/include/ $(pkg-config --cflags glib-2.0) -ldl -L. -lmimircache

int main(int argc, char **argv){
  reader_t* reader;

  csvReader_init_params* init_params = (void*) new_csvReader_init_params(6, -1, 1, -1, FALSE, '\t', -1);
  reader = setup_reader(argv[1], CSV_TRACE, OBJ_ID_STR, 0, 0, init_params);

  request_t* req = new_req_struct(OBJ_ID_STR);
  int i;
  read_one_element(reader, req);
  for (i = 0; i < 18 && req->valid; i++) {
    printf("read in %s, time %ld\n", req->obj_id, (long) req->real_time);
    read_one_element(reader, req);
  }

  cache_t *lru = create_cache("LRU", 20, OBJ_ID_STR, NULL);
  printf("%d\n", lru->core->add_element(lru, req));

  cache_t *mylru = create_cache("myLRU", 20, OBJ_ID_STR, NULL);
  printf("%d\n", mylru->core->add_element(mylru, req));

  destroy_req_struct(req);


  return 0;
}

