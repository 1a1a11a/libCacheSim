//
// Created by Juncheng Yang on 11/15/19.
//

#include "cacheSimulator.hpp"

using namespace std;
int main(int argc, char **argv){
  reader_t* reader;

  csvReader_init_params* init_params = (csvReader_init_params*) new_csvReader_init_params(6, -1, 1, -1, FALSE, '\t', -1);
  reader = setup_reader(argv[1], CSV_TRACE, OBJ_ID_STR, 0, 0, init_params);

  request_t* req = new_req_struct(OBJ_ID_STR);
  int i;
  read_one_element(reader, req);
  for (i = 0; i < 18 && req->valid; i++) {
    printf("read in %s, time %ld\n", req->obj_id, (long) req->real_time);
    read_one_element(reader, req);
  }

  string cache_name = "LRU";
  cache_t *lru = create_cache(cache_name.c_str(), 20, OBJ_ID_STR, NULL);
  printf("%d\n", lru->core->add_element(lru, req));
  printf("%d\n", lru->core->add_element(lru, req));

  cache_name = "myLRU";
  cache_t *mylru = create_cache(cache_name.c_str(), 20, OBJ_ID_STR, NULL);
  printf("%d\n", mylru->core->add_element(mylru, req));
  printf("%d\n", mylru->core->add_element(mylru, req));

//  cache_name = "myLRUCpp";
//  cache_t *mylru2 = create_cache(cache_name.c_str(), 20, OBJ_ID_STR, NULL);
//  printf("%d\n", mylru2->core->add_element(mylru2, req));
////  printf("size %d\n", mylru2->core->get_current_size(mylru2));
//  printf("%d\n", mylru2->core->add_element(mylru2, req));

  destroy_req_struct(req);


  return 0;
}

