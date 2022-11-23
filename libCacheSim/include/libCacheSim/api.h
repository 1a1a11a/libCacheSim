#pragma once

#include "cache.h"
#include "enum.h"
#include "reader.h"

static inline cache_stat_t go(cache_t *cache, reader_t *reader) {
  request_t *req = new_request();

  read_one_req(reader, req);
  int64_t start_ts = (int64_t)req->real_time;
  req->real_time -= start_ts;
  cache_stat_t stat;
  memset(&stat, 0, sizeof(stat));

  while (req->valid) {
    stat.n_req++;
    stat.n_req_byte += req->obj_size;

    if (cache->check(cache, req, true) != cache_ck_hit) {
      stat.n_miss++;
      stat.n_miss_byte += req->obj_size;

      bool admit = true;

      if (cache->admissioner != NULL) {
        admit = cache->admissioner->admit(cache->admissioner, req);
      }

      if (admit) {
        if (req->obj_size > cache->cache_size) {
          WARN("object %" PRIu64 ": obj size %" PRIu32
               " larger than cache size %" PRIu64 "\n",
               req->obj_id, req->obj_size, cache->cache_size);
        }

        while (cache->occupied_size + req->obj_size + cache->per_obj_metadata_size >
               cache->cache_size)
          cache->evict(cache, req, NULL);

        cache->insert(cache, req);
      }
    }
    read_one_req(reader, req);
    req->real_time -= start_ts;
  }

  stat.n_obj = cache->n_obj;
  stat.occupied_size = cache->occupied_size;
  return stat;
}