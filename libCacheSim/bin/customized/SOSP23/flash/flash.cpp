

#include <libgen.h>

#include <unordered_set>

#include "../../../../include/libCacheSim/evictionAlgo.h"
#include "flash.hpp"

#define REPORT_INTERVAL (4 * 3600)
#define MILLION 1000000ul

void calWriteAmp(reader_t *reader, cache_t *cache) {
  /* random seed */
  srand(time(NULL));
  set_rand_seed(rand());

  request_t *req = new_request();
  read_one_req(reader, req);
  uint64_t start_ts = (uint64_t)req->clock_time;
  req->clock_time = 0;

  std::unordered_set<uint64_t> seen;

  int64_t n_req = 0, n_byte = 0, n_uniq_obj = 0, n_uniq_byte = 0;
  int64_t n_miss = 0, n_miss_byte = 0;
  while (req->valid) {
    n_req++;
    n_byte += req->obj_size;
    if (seen.find(req->obj_id) == seen.end()) {
      seen.insert(req->obj_id);
      n_uniq_obj++;
      n_uniq_byte += req->obj_size;
    }

    if (cache->get(cache, req) == false) {
      n_miss += 1;
      n_miss_byte += req->obj_size;
    }

    read_one_req(reader, req);
    req->clock_time -= start_ts;
  }

  printf("%s %32s miss ratio %.4lf byte miss ratio %.4lf, ",
         basename(reader->trace_path), cache->cache_name,
         n_miss / (double)n_req, n_miss_byte / (double)n_byte);

  int64_t n_byte_write = n_miss_byte;
  if (strcasecmp("fifo", cache->cache_name) == 0) {
    ;
  } else if (strcasestr(cache->cache_name, "flashProb") != NULL) {
    flashProb_params_t *params = (flashProb_params_t *)cache->eviction_params;

    n_byte_write = params->n_byte_admit_to_disk;

    if (strcasestr(params->disk->cache_name, "Clock") != NULL) {
      Clock_params_t *clock_params =
          (Clock_params_t *)params->disk->eviction_params;
      n_byte_write += clock_params->n_byte_rewritten;
    }
  } else if (strcasecmp(cache->cache_name, "FIFO_Reinsertion") == 0) {
    FIFO_Reinsertion_params_t *params =
        (FIFO_Reinsertion_params_t *)cache->eviction_params;

    n_byte_write = n_miss_byte + params->n_byte_rewritten;
  } else if (strcasestr(cache->cache_name, "TinyLFU") != NULL) {
    WTinyLFU_params_t *params = (WTinyLFU_params_t *)cache->eviction_params;
    n_byte_write = params->n_admit_bytes;

    if (strcasestr(params->main_cache->cache_name, "Clock") != NULL) {
      Clock_params_t *clock_params =
          (Clock_params_t *)params->main_cache->eviction_params;
      n_byte_write += clock_params->n_byte_rewritten;
    } else if (strcasestr(params->main_cache->cache_name, "fifo") != NULL) {
      ;
    } else {
      printf("\n");
      ERROR("Main cache type only support FIFO and FIFO-Reinsertion (CLOCK)\n");
    }
  } else if (strcasestr(cache->cache_name, "qdlp") != NULL) {
    QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
    if (strcasestr(params->main_cache->cache_name, "Clock") != NULL) {
      Clock_params_t *clock_params =
          (Clock_params_t *)params->main_cache->eviction_params;
      n_byte_write =
          (params->n_byte_admit_to_main + params->n_byte_move_to_main +
           clock_params->n_byte_rewritten);
    } else if (strcasestr(params->main_cache->cache_name, "fifo") != NULL) {
      n_byte_write = params->n_byte_admit_to_main + params->n_byte_move_to_main;
    } else {
      printf("\n");
      ERROR("Main cache type only support FIFO and FIFO-Reinsertion (CLOCK)\n");
    }
  } else {
    printf("\n");
    ERROR("Do not support write amp for %s cache\n", cache->cache_name);
  }

  printf("write_amp %.4lf\n", n_byte_write / (double)n_uniq_byte);

  if (strcasestr(cache->cache_name, "qdlp") != NULL) {
    QDLPv1_params_t *params = (QDLPv1_params_t *)cache->eviction_params;
    Clock_params_t *clock_params =
        (Clock_params_t *)params->main_cache->eviction_params;

    printf("%.4lf/%.2lf/%.2lf/%.2lf\n",
           params->n_byte_admit_to_main / (double)params->n_byte_admit_to_fifo,
           params->n_byte_admit_to_main / (double)n_uniq_byte,
           params->n_byte_move_to_main / (double)n_uniq_byte,
           clock_params->n_byte_rewritten / (double)n_uniq_byte);
  }
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

  calWriteAmp(args.reader, args.caches[0]);

  free_arg(&args);

  return 0;
}
