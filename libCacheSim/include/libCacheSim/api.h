#pragma once

#include "cache.h"
#include "enum.h"
#include "reader.h"

static inline cache_stat_t go(cache_t *cache, reader_t *reader) {
  request_t *req = new_request();

  read_one_req(reader, req);
  int64_t start_ts = (int64_t)req->clock_time;
  req->clock_time -= start_ts;
  cache_stat_t stat;
  memset(&stat, 0, sizeof(stat));

  while (req->valid) {
    stat.n_req++;
    stat.n_req_byte += req->obj_size;

    if (cache->find(cache, req, true) == false) {
      stat.n_miss++;
      stat.n_miss_byte += req->obj_size;

      bool admit = true;

      if (cache->admissioner != NULL) {
        admit = cache->admissioner->admit(cache->admissioner, req);
      }

      if (admit) {
        if (req->obj_size > cache->cache_size) {
          WARN("object %" PRIu64 ": obj size %lu"
               " larger than cache size %" PRIu64 "\n",
               req->obj_id, req->obj_size, cache->cache_size);
        }

        while (cache->occupied_byte + req->obj_size + cache->obj_md_size >
               cache->cache_size)
          cache->evict(cache, req);

        cache->insert(cache, req);
      }
    }
    read_one_req(reader, req);
    req->clock_time -= start_ts;
  }

  stat.n_obj = cache->n_obj;
  stat.occupied_byte = cache->occupied_byte;
  return stat;
}