

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/libCacheSim/reader.h"
#include "internal.hpp"


int main(int argc, char *argv[]) {
  struct arguments args;

  cli::parse_cmd(argc, argv, &args);

  request_t *req = new_request();
  read_one_req(args.reader, req);
  
  bool trace_has_next_access_vtime = req->next_access_vtime != -2;

  if (trace_has_next_access_vtime) {
    printf("# time, object, size, next_access_vtime\n");
  } else {
    printf("# time, object, size\n");
  }

  while (req->valid) {
    printf("%ld, %lu, %d", (long)req->clock_time, (unsigned long)req->obj_id,
           (int)req->obj_size);
    if (trace_has_next_access_vtime) {
      printf(", %ld\n", (long)req->next_access_vtime);
    } else {
      printf("\n");
    }
    read_one_req(args.reader, req);
  }

  free_request(req);

  return 0;
}

