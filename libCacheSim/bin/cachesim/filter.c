

#include <libgen.h>

#include "../../include/libCacheSim/cache.h"
#include "../../include/libCacheSim/reader.h"
#include "../../utils/include/mymath.h"
#include "../../utils/include/mystr.h"
#include "../../utils/include/mysys.h"
#include "internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REPORT_INTERVAL (24 * 3600)

struct output_format {
  uint32_t clock_time;
  uint64_t obj_id;
  uint32_t obj_size;
  int64_t next_access_vtime;
} __attribute__((packed));

void filter(reader_t *reader, cache_t *cache) {
  /* random seed */
  srand(time(NULL));
  set_rand_seed(rand());

  char ofilepath[128];
  sprintf(ofilepath, "%s.filter%s", basename(reader->trace_path),
          cache->cache_name);
  request_t *req = new_request();

  FILE *output_file = fopen(ofilepath, "wb");
  struct output_format output_req;
  output_req.next_access_vtime = -2;

  read_one_req(reader, req);
  uint64_t start_ts = (uint64_t)req->clock_time;

  int64_t n_req = 0, n_written_req = 0;
  while (req->valid) {
    n_req++;
    req->clock_time -= start_ts;
    if (cache->get(cache, req) == false) {
      output_req.clock_time = req->clock_time;
      output_req.obj_id = req->obj_id;
      output_req.obj_size = req->obj_size;
      fwrite(&output_req, sizeof(struct output_format), 1, output_file);
      n_written_req++;
    }

    read_one_req(reader, req);
  }

  INFO("write %ld/%ld %.4lf requests to %s file\n", n_written_req, n_req,
       (double)n_written_req / n_req, ofilepath);
  fclose(output_file);
}

int main(int argc, char *argv[]) {
  struct arguments args;
  parse_cmd(argc, argv, &args);
  if (args.n_cache_size != 1) {
    WARN("only support one cache size\n");
    exit(0);
  }
  if (args.n_eviction_algo != 1) {
    WARN("only support one eviction algorithm\n");
    exit(0);
  }

  filter(args.reader, args.caches[0]);

  free_arg(&args);

  return 0;
}

#ifdef __cplusplus
}
#endif
