

#ifdef __linux__
#include <sys/sysinfo.h>
#endif
#include <assert.h>

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/dist.h"
#include "../../include/libCacheSim/reader.h"
#include "../../utils/include/mysys.h"
#include "internal.h"

int main(int argc, char **argv) {
  struct arguments args;
  parse_cmd(argc, argv, &args);

  int32_t *dist_array = NULL;
  int64_t array_size = 0;
  if (args.dist_type == STACK_DIST || args.dist_type == FUTURE_STACK_DIST) {
    dist_array = get_stack_dist(args.reader, args.dist_type, &array_size);
  } else if (args.dist_type == DIST_SINCE_LAST_ACCESS ||
             args.dist_type == DIST_SINCE_FIRST_ACCESS) {
    dist_array = get_access_dist(args.reader, args.dist_type, &array_size);
  } else {
    ERROR("Unknown distance type %d\n", args.dist_type);
  }

  if (strcasecmp(args.output_type, "binary") == 0) {
    save_dist(args.reader, dist_array, array_size, args.ofilepath,
              args.dist_type);

  } else if (strcasecmp(args.output_type, "txt") == 0) {
    save_dist_txt(args.reader, dist_array, array_size, args.ofilepath,
                  args.dist_type);
  } else if (strcasecmp(args.output_type, "cntTxt") == 0) {
    save_dist_as_cnt_txt(args.reader, dist_array, array_size, args.ofilepath,
                         args.dist_type);
  }

  return 0;
}
