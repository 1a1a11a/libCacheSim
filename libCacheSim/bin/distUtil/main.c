

#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <assert.h>

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/dist.h"
#include "../../utils/include/mysys.h"
#include "internal.h"

int main(int argc, char **argv) {
  struct arguments args;
  parse_cmd(argc, argv, &args);

  int32_t *dist_array = NULL;
  int64_t array_size = 0;
  if (args.dist_type == STACK_DIST) {
    dist_array = get_stack_dist(args.reader, args.dist_type, &array_size);
    save_dist_as_cnt(args.reader, dist_array, get_num_of_req(args.reader), args.ofilepath, args.dist_type);
  }
  return 0;
}



