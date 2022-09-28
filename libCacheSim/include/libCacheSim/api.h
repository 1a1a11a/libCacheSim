#pragma once

#include "cache.h"
#include "enum.h"
#include "reader.h"

static inline cache_stat_t go(cache_t *cache, reader_t *reader) {
  request_t *req = new_request();

  read_one_req(reader, req);
  int64_t start_ts = (int64_t)req->real_time;
  req->real_time -= start_ts;

  while (req->valid) {
    cache->stat.n_req++;
    cache->stat.n_req_byte += req->obj_size;

    if (cache->check(cache, req, true) != cache_ck_hit) {
      cache->stat.n_miss++;
      cache->stat.n_miss_byte += req->obj_size;

      bool admit = true;

      if (cache->admit != NULL && !!cache->admit(cache, req)) {
        admit = false;
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

  cache->stat.n_obj = cache->n_obj;
  cache->stat.occupied_size = cache->occupied_size;
  return cache->stat;
}