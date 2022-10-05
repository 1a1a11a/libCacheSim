//
// Created by Juncheng Yang on 11/15/19.
//

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include "libCacheSim.h"

int main(int argc, char **argv) {
  /* setup reader */
  reader_init_param_t init_params = {.obj_id_field = 5,
                                     .obj_size_field = 4,
                                     .time_field = 4,
                                     .has_header_set = true,
                                     .has_header = true,
                                     .delimiter = ','};
  reader_t *reader =
      setup_reader("../../../data/trace.csv", CSV_TRACE, &init_params);

  /* set up a request */
  request_t *req = new_request();

  /* read one request and print */
  read_one_req(reader, req);
  print_request(req);

  /* setup a cache */
  common_cache_params_t cc_params = {
      .cache_size = 1 * GiB, .hashpower = 24, .consider_obj_metadata = false};
  cache_t *lru = LRU_init(cc_params, NULL);

  int64_t n_hit = 0, n_req = 0;
  while (read_one_req(reader, req) == 0) {
    if (lru->get(lru, req) == cache_ck_hit) {
      n_hit++;
    }
    n_req++;
  }

  printf("hit ratio: %lf\n", (double)n_hit / n_req);

  free_request(req);
  lru->cache_free(lru);

  return 0;
}
