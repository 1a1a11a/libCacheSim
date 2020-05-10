//
// Created by Juncheng Yang on 11/15/19.
//

#include "cacheSimulator.hpp"

using namespace std;
int main(int argc, char **argv){
  reader_init_param_t init_params = {.obj_id_field=6, .has_header=FALSE, .delimiter='\t'};
  reader_t *reader = setup_reader("../../data/trace.csv", CSV_TRACE, OBJ_ID_STR, &init_params);
  request_t* req = new_request(OBJ_ID_STR);
  read_one_req(reader, req);
  for (int i = 0; i < 18 && req->valid; i++) {
    printf("read in %s, time %ld\n", (gchar*) req->obj_id_ptr, (long) req->real_time);
    read_one_req(reader, req);
  }

  string cache_name = "LRU";
  cache_t *lru = create_cache(cache_name.c_str(), 20, OBJ_ID_STR, nullptr);
  printf("%d\n", lru->core.get(lru, req));
  printf("%d\n", lru->core.get(lru, req));

  cache_name = "myLRU";
  cache_t *mylru = create_cache(cache_name.c_str(), 20, OBJ_ID_STR, nullptr);
  printf("%d\n", mylru->core.add_element(mylru, req));
  printf("%d\n", mylru->core.add_element(mylru, req));

//  cache_name = "myLRUCpp";
//  cache_t *mylru2 = create_cache(cache_name.c_str(), 20, OBJ_ID_STR, NULL);
//  printf("%d\n", mylru2->core.add_element(mylru2, req));
////  printf("size %d\n", mylru2->core.get_current_size(mylru2));
//  printf("%d\n", mylru2->core.add_element(mylru2, req));

  destroy_req_struct(req);


  return 0;
}

